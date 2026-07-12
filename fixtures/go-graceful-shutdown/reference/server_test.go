package shutdown

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"
)

func TestSubmitShutdownRaceDoesNotPanic(t *testing.T) {
	server, err := NewServer(
		func(context.Context, Job) error { return nil },
		Options{QueueCapacity: 32},
	)
	if err != nil {
		t.Fatal(err)
	}

	start := make(chan struct{})
	var requests sync.WaitGroup
	for index := 0; index < 32; index++ {
		requests.Add(1)
		go func() {
			defer requests.Done()
			<-start
			request := httptest.NewRequest(
				http.MethodPost,
				"/jobs",
				strings.NewReader(`{"id":"race"}`),
			)
			request.Header.Set("Content-Type", "application/json")
			response := httptest.NewRecorder()
			server.Handler().ServeHTTP(response, request)
			if response.Code != http.StatusAccepted &&
				response.Code != http.StatusServiceUnavailable {
				t.Errorf("status = %d, want 202 or 503", response.Code)
			}
		}()
	}
	close(start)
	if err := server.Shutdown(context.Background()); err != nil {
		t.Fatal(err)
	}
	requests.Wait()
}

func TestShutdownDeadlineCancelsProcessor(t *testing.T) {
	started := make(chan struct{})
	server, err := NewServer(func(ctx context.Context, _ Job) error {
		close(started)
		<-ctx.Done()
		return ctx.Err()
	}, Options{QueueCapacity: 1})
	if err != nil {
		t.Fatal(err)
	}

	request := httptest.NewRequest(
		http.MethodPost,
		"/jobs",
		strings.NewReader(`{"id":"blocked"}`),
	)
	request.Header.Set("Content-Type", "application/json")
	response := httptest.NewRecorder()
	server.Handler().ServeHTTP(response, request)
	if response.Code != http.StatusAccepted {
		t.Fatalf("status = %d, want 202", response.Code)
	}
	<-started

	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Millisecond)
	defer cancel()
	if err := server.Shutdown(ctx); !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("Shutdown error = %v, want deadline exceeded", err)
	}
}
