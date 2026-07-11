# Starter Contract

The candidate is one Go source file declaring package `shutdown`. It must
export the following API:

```go
const WorkerCount = 4
const DefaultShutdownTimeout = 10 * time.Second

var ErrShuttingDown error
var ErrAlreadyServing error

type Job struct {
    ID      string `json:"id"`
    Payload string `json:"payload"`
}

type Processor func(context.Context, Job) error

type Options struct {
    QueueCapacity  int
    ShutdownTimeout time.Duration
    OnError        func(error)
}

func NewServer(Processor, Options) (*Server, error)
func (*Server) Handler() http.Handler
func (*Server) Serve(net.Listener) error
func (*Server) Shutdown(context.Context) error
func (*Server) RunContext(context.Context, net.Listener) error
func (*Server) Run(net.Listener) error
```

`POST /jobs` accepts exactly one JSON object with a nonempty `id`, returns
202 only after enqueueing it, returns 429 when the bounded queue is full, and
returns 503 after shutdown admission closes. Other methods return 405 and
invalid media types or bodies return 415 or 400 respectively.

Exactly four workers call `Processor`. Processor errors and panics are sent to
`OnError` without terminating a worker. `Shutdown` rejects new work before it
closes the queue, stops the HTTP server, waits for every accepted job, and
returns the caller context error if its deadline expires. Deadline expiry must
cancel the context passed to processors. `RunContext` uses the fixed ten-second
shutdown timeout after its context is canceled unless `ShutdownTimeout` is set
to a positive test duration; negative durations are invalid. `Run` converts
SIGINT and SIGTERM into that cancellation.

Candidate packages may not define `init` functions or reference direct process
termination functions such as `os.Exit`, `syscall.Exit`, `runtime.Goexit`, or
the `log.Fatal` family. The validator enforces this before compilation so
package initialization cannot bypass the public tests.

This remains a validator scaffold. The benchmark prompt has a multi-file
answer contract, so extraction cannot be activated until validated answer
bundles are supported.
