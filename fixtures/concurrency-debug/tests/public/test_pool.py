import pathlib
import secrets
import subprocess
import sys
import textwrap
import unittest


ANSWER_PATH = pathlib.Path(__file__).resolve().parents[2] / "generated/answer.py"
LOADER = """
import importlib.util
import pathlib
import sys

answer_path = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("candidate_pool", answer_path)
if spec is None or spec.loader is None:
    raise RuntimeError("candidate module cannot be loaded")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
Pool = module.Pool
"""


class PoolTests(unittest.TestCase):
    def run_scenario(self, source):
        completion_marker = f"scenario-complete-{secrets.token_hex(16)}"
        script = (
            LOADER + "\n" + textwrap.dedent(source) +
            f"\nprint({completion_marker!r}, flush=True)\n"
        )
        try:
            result = subprocess.run(
                [sys.executable, "-c", script, str(ANSWER_PATH)],
                check=False,
                capture_output=True,
                text=True,
                timeout=2,
            )
        except subprocess.TimeoutExpired:
            self.fail("scenario timed out; the pool deadlocked or leaked a worker")
        self.assertEqual(
            result.returncode,
            0,
            msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn(
            completion_marker,
            result.stdout.splitlines(),
            msg="scenario exited without reaching its completion marker",
        )

    def test_accepted_work_finishes_and_late_submit_is_rejected(self):
        self.run_scenario("""
            import threading

            pool = Pool(3)
            completed = []
            lock = threading.Lock()

            def record(value):
                with lock:
                    completed.append(value)

            for value in range(40):
                result = pool.submit(lambda value=value: record(value))
                assert result is None
            pool.shutdown()
            assert sorted(completed) == list(range(40))
            try:
                pool.submit(lambda: None)
            except RuntimeError:
                pass
            else:
                raise AssertionError("submit was accepted after shutdown")
        """)

    def test_concurrent_submit_and_shutdown_account_for_every_acceptance(self):
        self.run_scenario("""
            import threading

            pool = Pool(4)
            producer_count = 12
            start = threading.Barrier(producer_count + 1)
            accepted = []
            completed = []
            lock = threading.Lock()

            def task(value):
                with lock:
                    completed.append(value)

            def producer(value):
                start.wait()
                try:
                    pool.submit(lambda: task(value))
                except RuntimeError:
                    return
                with lock:
                    accepted.append(value)

            producers = [
                threading.Thread(target=producer, args=(value,))
                for value in range(producer_count)
            ]
            for producer in producers:
                producer.start()
            start.wait()
            pool.shutdown()
            for producer in producers:
                producer.join()
            assert sorted(completed) == sorted(accepted)
        """)

    def test_invalid_worker_counts_are_rejected(self):
        self.run_scenario("""
            for count in (0, -1):
                try:
                    Pool(count)
                except BaseException as error:
                    assert type(error) is ValueError
                else:
                    raise AssertionError(f"accepted worker count {count}")

            for count in (1.5, "2"):
                try:
                    pool = Pool(count)
                except BaseException as error:
                    assert type(error) is TypeError
                else:
                    pool.shutdown()
                    raise AssertionError(f"accepted non-integer count {count!r}")
        """)

    def test_shutdown_is_idempotent_and_concurrent_callers_return(self):
        self.run_scenario("""
            import threading

            pool = Pool(2)
            release = threading.Event()
            pool.submit(release.wait)
            failures = []
            any_returned = threading.Event()
            start = threading.Barrier(3)

            def stop():
                try:
                    start.wait()
                    pool.shutdown()
                except BaseException as error:
                    failures.append(error)
                finally:
                    any_returned.set()

            callers = [
                threading.Thread(target=stop)
                for _ in range(2)
            ]
            for caller in callers:
                caller.start()
            start.wait()
            early_return = any_returned.wait(0.2)
            release.set()
            for caller in callers:
                caller.join()
            pool.shutdown()
            assert not failures
            assert not early_return
        """)

    def test_submit_requires_a_callable(self):
        self.run_scenario("""
            pool = Pool(1)
            rejected = False
            try:
                pool.submit(None)
            except TypeError:
                rejected = True
            pool.shutdown()
            assert rejected
        """)

    def test_task_exception_does_not_kill_worker_or_break_accounting(self):
        self.run_scenario("""
            import threading

            pool = Pool(1)
            completed = threading.Event()

            def fail():
                raise RuntimeError("expected task failure")

            pool.submit(fail)
            pool.submit(completed.set)
            pool.shutdown()
            assert completed.is_set()
        """)

    def test_worker_shutdown_initiates_without_waiting_on_itself(self):
        self.run_scenario("""
            import threading

            pool = Pool(2)
            returned = threading.Event()
            start_shutdown = threading.Event()
            completed = []
            lock = threading.Lock()

            def stop_from_worker():
                start_shutdown.wait()
                pool.shutdown()
                returned.set()

            def record(value):
                with lock:
                    completed.append(value)

            pool.submit(stop_from_worker)
            for value in range(20):
                pool.submit(lambda value=value: record(value))
            start_shutdown.set()
            assert returned.wait(1)
            pool.shutdown()
            assert sorted(completed) == list(range(20))
        """)


if __name__ == "__main__":
    unittest.main()
