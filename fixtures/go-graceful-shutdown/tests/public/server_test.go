package publictests

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"testing"
	"time"

	shutdown "fixture.local/go-graceful-shutdown/generated"
)

const (
	completionEnvironment = "FIXTURE_COMPLETION_TOKEN"
	completionPrefix      = "fixture-complete:"
	testWait              = 750 * time.Millisecond
)

func TestMain(m *testing.M) {
	code := m.Run()
	if code == 0 {
		token := os.Getenv(completionEnvironment)
		if token == "" {
			fmt.Fprintln(os.Stderr, "missing fixture completion token")
			code = 1
		} else {
			fmt.Println(completionPrefix + token)
		}
	}
	os.Exit(code)
}

func TestConstructorAndRequestValidation(t *testing.T) {
	if shutdown.WorkerCount != 4 {
		t.Fatalf("WorkerCount = %d, want 4", shutdown.WorkerCount)
	}
	if shutdown.DefaultShutdownTimeout != 10*time.Second {
		t.Fatalf(
			"DefaultShutdownTimeout = %s, want 10s",
			shutdown.DefaultShutdownTimeout,
		)
	}
	if _, err := shutdown.NewServer(nil, shutdown.Options{QueueCapacity: 1}); err == nil {
		t.Fatal("NewServer accepted a nil processor")
	}
	if _, err := shutdown.NewServer(
		func(context.Context, shutdown.Job) error { return nil },
		shutdown.Options{QueueCapacity: 0},
	); err == nil {
		t.Fatal("NewServer accepted a nonpositive queue capacity")
	}
	if _, err := shutdown.NewServer(
		func(context.Context, shutdown.Job) error { return nil },
		shutdown.Options{
			QueueCapacity:   1,
			ShutdownTimeout: -time.Millisecond,
		},
	); err == nil {
		t.Fatal("NewServer accepted a negative shutdown timeout")
	}

	server := mustServer(t, 4, func(context.Context, shutdown.Job) error {
		return nil
	}, nil)
	handler := server.Handler()
	for _, testCase := range []struct {
		name        string
		method      string
		path        string
		body        string
		contentType string
		want        int
	}{
		{name: "not found", method: http.MethodPost, path: "/other", body: `{}`, contentType: "application/json", want: http.StatusNotFound},
		{name: "wrong method", method: http.MethodGet, path: "/jobs", contentType: "application/json", want: http.StatusMethodNotAllowed},
		{name: "wrong media type", method: http.MethodPost, path: "/jobs", body: `{}`, contentType: "text/plain", want: http.StatusUnsupportedMediaType},
		{name: "malformed", method: http.MethodPost, path: "/jobs", body: `{`, contentType: "application/json", want: http.StatusBadRequest},
		{name: "missing id", method: http.MethodPost, path: "/jobs", body: `{"payload":"x"}`, contentType: "application/json", want: http.StatusBadRequest},
		{name: "blank id", method: http.MethodPost, path: "/jobs", body: `{"id":"  "}`, contentType: "application/json", want: http.StatusBadRequest},
		{name: "unknown field", method: http.MethodPost, path: "/jobs", body: `{"id":"a","extra":true}`, contentType: "application/json", want: http.StatusBadRequest},
		{name: "trailing value", method: http.MethodPost, path: "/jobs", body: `{"id":"a"} {"id":"b"}`, contentType: "application/json", want: http.StatusBadRequest},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			response := performRequest(
				handler,
				testCase.method,
				testCase.path,
				testCase.body,
				testCase.contentType,
			)
			if response.Code != testCase.want {
				t.Fatalf("status = %d, want %d", response.Code, testCase.want)
			}
			if testCase.want == http.StatusMethodNotAllowed &&
				response.Header().Get("Allow") != http.MethodPost {
				t.Fatalf("Allow = %q, want POST", response.Header().Get("Allow"))
			}
		})
	}
	shutdownServer(t, server)
}

func TestBoundedQueueAndFourWorkers(t *testing.T) {
	started := make(chan struct{}, shutdown.WorkerCount)
	release := make(chan struct{})
	var releaseOnce sync.Once
	releaseWorkers := func() {
		releaseOnce.Do(func() { close(release) })
	}
	defer releaseWorkers()

	server := mustServer(t, 1, func(context.Context, shutdown.Job) error {
		started <- struct{}{}
		<-release
		return nil
	}, nil)
	handler := server.Handler()
	for index := 0; index < shutdown.WorkerCount; index++ {
		response := submit(handler, fmt.Sprintf("worker-%d", index))
		if response.Code != http.StatusAccepted {
			t.Fatalf("worker submission %d status = %d", index, response.Code)
		}
		waitSignal(t, started, "worker did not start")
	}
	if response := submit(handler, "queued"); response.Code != http.StatusAccepted {
		t.Fatalf("queued job status = %d, want 202", response.Code)
	}
	if response := submit(handler, "full"); response.Code != http.StatusTooManyRequests {
		t.Fatalf("full queue status = %d, want 429", response.Code)
	}

	releaseWorkers()
	shutdownServer(t, server)
}

func TestShutdownWaitsForAcceptedWork(t *testing.T) {
	started := make(chan struct{}, 1)
	release := make(chan struct{})
	server := mustServer(t, 2, func(context.Context, shutdown.Job) error {
		started <- struct{}{}
		<-release
		return nil
	}, nil)
	if response := submit(server.Handler(), "accepted"); response.Code != http.StatusAccepted {
		t.Fatalf("submission status = %d, want 202", response.Code)
	}
	waitSignal(t, started, "accepted job did not start")

	shutdownDone := make(chan error, 1)
	go func() {
		shutdownDone <- server.Shutdown(context.Background())
	}()
	select {
	case err := <-shutdownDone:
		t.Fatalf("Shutdown returned before accepted work finished: %v", err)
	case <-time.After(40 * time.Millisecond):
	}
	close(release)
	if err := waitError(t, shutdownDone, "Shutdown did not finish after work drained"); err != nil {
		t.Fatalf("Shutdown returned %v", err)
	}
	if response := submit(server.Handler(), "late"); response.Code != http.StatusServiceUnavailable {
		t.Fatalf("late submission status = %d, want 503", response.Code)
	}
	shutdownServer(t, server)
}

func TestConcurrentSubmitAndShutdownAccountForAcceptedJobs(t *testing.T) {
	var processed atomic.Int64
	server := mustServer(t, 256, func(context.Context, shutdown.Job) error {
		processed.Add(1)
		return nil
	}, nil)
	handler := server.Handler()

	const submissions = 128
	start := make(chan struct{})
	statuses := make(chan int, submissions)
	var submitters sync.WaitGroup
	for index := 0; index < submissions; index++ {
		submitters.Add(1)
		go func(index int) {
			defer submitters.Done()
			<-start
			statuses <- submit(handler, fmt.Sprintf("race-%d", index)).Code
		}(index)
	}
	shutdownDone := make(chan error, 1)
	go func() {
		<-start
		shutdownDone <- server.Shutdown(context.Background())
	}()
	close(start)
	submitters.Wait()
	close(statuses)
	if err := waitError(t, shutdownDone, "concurrent Shutdown did not finish"); err != nil {
		t.Fatalf("Shutdown returned %v", err)
	}

	accepted := int64(0)
	for status := range statuses {
		switch status {
		case http.StatusAccepted:
			accepted++
		case http.StatusServiceUnavailable:
		default:
			t.Fatalf("race submission returned unexpected status %d", status)
		}
	}
	if got := processed.Load(); got != accepted {
		t.Fatalf("processed %d jobs, want exactly %d accepted jobs", got, accepted)
	}
	if response := submit(handler, "after-race"); response.Code != http.StatusServiceUnavailable {
		t.Fatalf("post-shutdown status = %d, want 503", response.Code)
	}
	shutdownServer(t, server)
}

func TestShutdownDeadlineCancelsProcessors(t *testing.T) {
	started := make(chan struct{}, 1)
	canceled := make(chan struct{}, 1)
	release := make(chan struct{})
	defer close(release)
	server := mustServer(t, 1, func(ctx context.Context, _ shutdown.Job) error {
		started <- struct{}{}
		select {
		case <-ctx.Done():
			canceled <- struct{}{}
			return ctx.Err()
		case <-release:
			return nil
		}
	}, nil)
	if response := submit(server.Handler(), "deadline"); response.Code != http.StatusAccepted {
		t.Fatalf("submission status = %d, want 202", response.Code)
	}
	waitSignal(t, started, "deadline job did not start")

	ctx, cancel := context.WithTimeout(context.Background(), 40*time.Millisecond)
	defer cancel()
	err := server.Shutdown(ctx)
	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("Shutdown error = %v, want context deadline exceeded", err)
	}
	waitSignal(t, canceled, "processor context was not canceled at the deadline")
	shutdownServer(t, server)
}

func TestProcessorFailuresDoNotStopWorkers(t *testing.T) {
	var completed atomic.Int64
	var reported atomic.Int64
	processor := func(_ context.Context, job shutdown.Job) error {
		defer completed.Add(1)
		switch job.ID {
		case "error":
			return errors.New("expected processor error")
		case "panic":
			panic("expected processor panic")
		default:
			return nil
		}
	}
	server := mustServer(t, 16, processor, func(error) {
		reported.Add(1)
		panic("error callback panic must be contained")
	})
	for _, id := range []string{"error", "panic", "one", "two", "three", "four"} {
		if response := submit(server.Handler(), id); response.Code != http.StatusAccepted {
			t.Fatalf("submission %q status = %d", id, response.Code)
		}
	}
	shutdownServer(t, server)
	if got := completed.Load(); got != 6 {
		t.Fatalf("completed jobs = %d, want 6", got)
	}
	if got := reported.Load(); got != 2 {
		t.Fatalf("reported failures = %d, want 2", got)
	}
}

func TestServeStopsListenerAndRejectsSecondServe(t *testing.T) {
	server := mustServer(t, 1, func(context.Context, shutdown.Job) error {
		return nil
	}, nil)
	listener := newBlockingListener()
	serveDone := make(chan error, 1)
	go func() { serveDone <- server.Serve(listener) }()
	waitClosed(t, listener.accepted, "Serve did not call Accept")

	second := newBlockingListener()
	defer second.Close()
	if err := server.Serve(second); !errors.Is(err, shutdown.ErrAlreadyServing) {
		t.Fatalf("second Serve error = %v, want ErrAlreadyServing", err)
	}
	shutdownServer(t, server)
	if err := waitError(t, serveDone, "Serve did not return after Shutdown"); err != nil {
		t.Fatalf("Serve returned %v", err)
	}
}

func TestRunContextStopsOnCancellation(t *testing.T) {
	server := mustServer(t, 1, func(context.Context, shutdown.Job) error {
		return nil
	}, nil)
	listener := newBlockingListener()
	ctx, cancel := context.WithCancel(context.Background())
	runDone := make(chan error, 1)
	go func() { runDone <- server.RunContext(ctx, listener) }()
	waitClosed(t, listener.accepted, "RunContext did not start serving")
	cancel()
	if err := waitError(t, runDone, "RunContext did not stop after cancellation"); err != nil {
		t.Fatalf("RunContext returned %v", err)
	}
}

func TestRunContextUsesConfiguredShutdownTimeout(t *testing.T) {
	started := make(chan struct{}, 1)
	canceled := make(chan struct{}, 1)
	server, err := shutdown.NewServer(
		func(ctx context.Context, _ shutdown.Job) error {
			started <- struct{}{}
			<-ctx.Done()
			canceled <- struct{}{}
			return ctx.Err()
		},
		shutdown.Options{
			QueueCapacity:   1,
			ShutdownTimeout: 50 * time.Millisecond,
		},
	)
	if err != nil {
		t.Fatalf("NewServer: %v", err)
	}
	if response := submit(server.Handler(), "run-deadline"); response.Code != http.StatusAccepted {
		t.Fatalf("submission status = %d, want 202", response.Code)
	}
	waitSignal(t, started, "RunContext deadline job did not start")

	listener := newBlockingListener()
	ctx, cancel := context.WithCancel(context.Background())
	runDone := make(chan error, 1)
	go func() { runDone <- server.RunContext(ctx, listener) }()
	waitClosed(t, listener.accepted, "RunContext did not start serving")
	startedAt := time.Now()
	cancel()
	runError := waitError(t, runDone, "RunContext ignored its shutdown timeout")
	elapsed := time.Since(startedAt)
	if !errors.Is(runError, context.DeadlineExceeded) {
		t.Fatalf("RunContext error = %v, want context deadline exceeded", runError)
	}
	if elapsed < 30*time.Millisecond || elapsed > 500*time.Millisecond {
		t.Fatalf("RunContext elapsed time = %s, want configured 50ms timeout", elapsed)
	}
	waitSignal(t, canceled, "RunContext deadline did not cancel processor")
	shutdownServer(t, server)
}

func TestRunPropagatesSignals(t *testing.T) {
	for _, testCase := range []struct {
		name   string
		signal syscall.Signal
	}{
		{name: "SIGINT", signal: syscall.SIGINT},
		{name: "SIGTERM", signal: syscall.SIGTERM},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			server := mustServer(t, 1, func(context.Context, shutdown.Job) error {
				return nil
			}, nil)
			listener := newBlockingListener()
			runDone := make(chan error, 1)
			go func() { runDone <- server.Run(listener) }()
			waitClosed(t, listener.accepted, "Run did not start serving")
			if err := syscall.Kill(os.Getpid(), testCase.signal); err != nil {
				t.Fatalf("send %s: %v", testCase.name, err)
			}
			if err := waitError(t, runDone, "Run did not stop after signal"); err != nil {
				t.Fatalf("Run returned %v", err)
			}
		})
	}
}

func mustServer(
	t *testing.T,
	capacity int,
	processor shutdown.Processor,
	onError func(error),
) *shutdown.Server {
	t.Helper()
	server, err := shutdown.NewServer(processor, shutdown.Options{
		QueueCapacity: capacity,
		OnError:       onError,
	})
	if err != nil {
		t.Fatalf("NewServer: %v", err)
	}
	return server
}

func shutdownServer(t *testing.T, server *shutdown.Server) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), testWait)
	defer cancel()
	if err := server.Shutdown(ctx); err != nil {
		t.Fatalf("Shutdown: %v", err)
	}
}

func submit(handler http.Handler, id string) *httptest.ResponseRecorder {
	return performRequest(
		handler,
		http.MethodPost,
		"/jobs",
		fmt.Sprintf(`{"id":%q,"payload":"payload"}`, id),
		"application/json; charset=utf-8",
	)
}

func performRequest(
	handler http.Handler,
	method string,
	path string,
	body string,
	contentType string,
) *httptest.ResponseRecorder {
	request := httptest.NewRequest(method, path, strings.NewReader(body))
	if contentType != "" {
		request.Header.Set("Content-Type", contentType)
	}
	response := httptest.NewRecorder()
	handler.ServeHTTP(response, request)
	return response
}

func waitSignal(t *testing.T, signal <-chan struct{}, failure string) {
	t.Helper()
	select {
	case <-signal:
	case <-time.After(testWait):
		t.Fatal(failure)
	}
}

func waitClosed(t *testing.T, signal <-chan struct{}, failure string) {
	t.Helper()
	select {
	case <-signal:
	case <-time.After(testWait):
		t.Fatal(failure)
	}
}

func waitError(t *testing.T, result <-chan error, failure string) error {
	t.Helper()
	select {
	case err := <-result:
		return err
	case <-time.After(testWait):
		t.Fatal(failure)
		return nil
	}
}

type blockingListener struct {
	accepted chan struct{}
	closed   chan struct{}
	accept   sync.Once
	close    sync.Once
}

func newBlockingListener() *blockingListener {
	return &blockingListener{
		accepted: make(chan struct{}),
		closed:   make(chan struct{}),
	}
}

func (listener *blockingListener) Accept() (net.Conn, error) {
	listener.accept.Do(func() { close(listener.accepted) })
	<-listener.closed
	return nil, net.ErrClosed
}

func (listener *blockingListener) Close() error {
	listener.close.Do(func() { close(listener.closed) })
	return nil
}

func (listener *blockingListener) Addr() net.Addr {
	return fixtureAddress("fixture")
}

type fixtureAddress string

func (address fixtureAddress) Network() string { return string(address) }
func (address fixtureAddress) String() string  { return string(address) }
