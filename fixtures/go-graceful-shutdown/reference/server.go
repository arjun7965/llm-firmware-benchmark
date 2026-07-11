package shutdown

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"mime"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"sync"
	"syscall"
	"time"
)

const (
	WorkerCount            = 4
	DefaultShutdownTimeout = 10 * time.Second
	maximumRequestBytes    = 1 << 20
)

var (
	ErrShuttingDown   = errors.New("server is shutting down")
	ErrAlreadyServing = errors.New("server is already serving")
)

type Job struct {
	ID      string `json:"id"`
	Payload string `json:"payload"`
}

type Processor func(context.Context, Job) error

type Options struct {
	QueueCapacity   int
	ShutdownTimeout time.Duration
	OnError         func(error)
}

type Server struct {
	processor       Processor
	onError         func(error)
	jobs            chan Job
	shutdownTimeout time.Duration

	admissionMu sync.RWMutex
	accepting   bool
	stopOnce    sync.Once

	workerContext context.Context
	cancelWorkers context.CancelFunc
	workerGroup   sync.WaitGroup
	workersDone   chan struct{}

	httpMu     sync.Mutex
	httpServer *http.Server
}

func NewServer(processor Processor, options Options) (*Server, error) {
	if processor == nil {
		return nil, errors.New("processor is required")
	}
	if options.QueueCapacity <= 0 {
		return nil, errors.New("queue capacity must be positive")
	}
	if options.ShutdownTimeout < 0 {
		return nil, errors.New("shutdown timeout cannot be negative")
	}
	shutdownTimeout := options.ShutdownTimeout
	if shutdownTimeout == 0 {
		shutdownTimeout = DefaultShutdownTimeout
	}

	workerContext, cancelWorkers := context.WithCancel(context.Background())
	server := &Server{
		processor:       processor,
		onError:         options.OnError,
		jobs:            make(chan Job, options.QueueCapacity),
		shutdownTimeout: shutdownTimeout,
		accepting:       true,
		workerContext:   workerContext,
		cancelWorkers:   cancelWorkers,
		workersDone:     make(chan struct{}),
	}
	server.workerGroup.Add(WorkerCount)
	for worker := 0; worker < WorkerCount; worker++ {
		go server.work()
	}
	go func() {
		server.workerGroup.Wait()
		close(server.workersDone)
	}()
	return server, nil
}

func (s *Server) Handler() http.Handler {
	return http.HandlerFunc(s.handleJobs)
}

func (s *Server) Serve(listener net.Listener) error {
	if listener == nil {
		return errors.New("listener is required")
	}

	s.admissionMu.RLock()
	if !s.accepting {
		s.admissionMu.RUnlock()
		return ErrShuttingDown
	}
	s.httpMu.Lock()
	if s.httpServer != nil {
		s.httpMu.Unlock()
		s.admissionMu.RUnlock()
		return ErrAlreadyServing
	}
	httpServer := &http.Server{
		Handler:           s.Handler(),
		ReadHeaderTimeout: 5 * time.Second,
	}
	s.httpServer = httpServer
	s.httpMu.Unlock()
	s.admissionMu.RUnlock()

	err := httpServer.Serve(listener)
	if errors.Is(err, http.ErrServerClosed) {
		return nil
	}
	s.beginShutdown()
	return err
}

func (s *Server) Shutdown(ctx context.Context) error {
	if ctx == nil {
		return errors.New("shutdown context is required")
	}
	s.beginShutdown()

	s.httpMu.Lock()
	httpServer := s.httpServer
	s.httpMu.Unlock()
	var shutdownError error
	if httpServer != nil {
		shutdownError = httpServer.Shutdown(ctx)
	}

	select {
	case <-s.workersDone:
		s.cancelWorkers()
		return shutdownError
	case <-ctx.Done():
		s.cancelWorkers()
		return errors.Join(shutdownError, ctx.Err())
	}
}

func (s *Server) RunContext(ctx context.Context, listener net.Listener) error {
	if ctx == nil {
		return errors.New("run context is required")
	}
	serveDone := make(chan error, 1)
	go func() {
		serveDone <- s.Serve(listener)
	}()

	select {
	case serveError := <-serveDone:
		shutdownContext, cancel := context.WithTimeout(
			context.Background(),
			s.shutdownTimeout,
		)
		defer cancel()
		return errors.Join(serveError, s.Shutdown(shutdownContext))
	case <-ctx.Done():
		shutdownContext, cancel := context.WithTimeout(
			context.Background(),
			s.shutdownTimeout,
		)
		defer cancel()
		shutdownError := s.Shutdown(shutdownContext)
		serveError := <-serveDone
		return errors.Join(shutdownError, serveError)
	}
}

func (s *Server) Run(listener net.Listener) error {
	ctx, stop := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer stop()
	return s.RunContext(ctx, listener)
}

func (s *Server) beginShutdown() {
	s.stopOnce.Do(func() {
		s.admissionMu.Lock()
		s.accepting = false
		close(s.jobs)
		s.admissionMu.Unlock()
	})
}

func (s *Server) work() {
	defer s.workerGroup.Done()
	for job := range s.jobs {
		s.process(job)
	}
}

func (s *Server) process(job Job) {
	defer func() {
		if recovered := recover(); recovered != nil {
			s.reportError(fmt.Errorf("processor panic: %v", recovered))
		}
	}()
	if err := s.processor(s.workerContext, job); err != nil {
		s.reportError(err)
	}
}

func (s *Server) reportError(err error) {
	if s.onError == nil {
		return
	}
	defer func() {
		_ = recover()
	}()
	s.onError(err)
}

func (s *Server) handleJobs(writer http.ResponseWriter, request *http.Request) {
	if request.URL.Path != "/jobs" {
		http.NotFound(writer, request)
		return
	}
	if request.Method != http.MethodPost {
		writer.Header().Set("Allow", http.MethodPost)
		http.Error(writer, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	mediaType, _, err := mime.ParseMediaType(request.Header.Get("Content-Type"))
	if err != nil || mediaType != "application/json" {
		http.Error(writer, "content type must be application/json", http.StatusUnsupportedMediaType)
		return
	}

	request.Body = http.MaxBytesReader(writer, request.Body, maximumRequestBytes)
	defer request.Body.Close()
	decoder := json.NewDecoder(request.Body)
	decoder.DisallowUnknownFields()
	var job Job
	if err := decoder.Decode(&job); err != nil {
		http.Error(writer, "invalid job", http.StatusBadRequest)
		return
	}
	if err := requireJSONEnd(decoder); err != nil || strings.TrimSpace(job.ID) == "" {
		http.Error(writer, "invalid job", http.StatusBadRequest)
		return
	}

	s.admissionMu.RLock()
	defer s.admissionMu.RUnlock()
	if !s.accepting {
		http.Error(writer, "server is shutting down", http.StatusServiceUnavailable)
		return
	}
	select {
	case s.jobs <- job:
		writer.WriteHeader(http.StatusAccepted)
	default:
		http.Error(writer, "job queue is full", http.StatusTooManyRequests)
	}
}

func requireJSONEnd(decoder *json.Decoder) error {
	var extra any
	if err := decoder.Decode(&extra); !errors.Is(err, io.EOF) {
		if err == nil {
			return errors.New("multiple JSON values")
		}
		return err
	}
	return nil
}
