import queue
import threading


class Pool:
    def __init__(self, n):
        if not isinstance(n, int):
            raise TypeError("n must be an integer")
        if n <= 0:
            raise ValueError("n must be positive")

        self._queue = queue.Queue()
        self._state_lock = threading.Lock()
        self._closed = False
        self._sentinel = object()
        self._threads = [
            threading.Thread(target=self._worker, name=f"pool-worker-{index}")
            for index in range(n)
        ]
        for thread in self._threads:
            thread.start()

    def _worker(self):
        while True:
            item = self._queue.get()
            try:
                if item is self._sentinel:
                    return
                try:
                    item()
                except BaseException:
                    pass
            finally:
                self._queue.task_done()

    def submit(self, fn):
        with self._state_lock:
            if self._closed:
                raise RuntimeError("pool is shutting down")
            if not callable(fn):
                raise TypeError("fn must be callable")
            self._queue.put(fn)

    def shutdown(self):
        with self._state_lock:
            if not self._closed:
                self._closed = True
                for _ in self._threads:
                    self._queue.put(self._sentinel)

        if threading.current_thread() in self._threads:
            return

        self._queue.join()
        for thread in self._threads:
            thread.join()
