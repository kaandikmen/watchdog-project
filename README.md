# watchdog-project

POSIX Signal-Based Process Monitoring and Management System
(Sakarya University, Software Engineering — System Programming Term Project 1, 2026-2027)

The `watchdog` supervisor starts several worker processes with `fork()` + `exec()`,
detects their termination via `SIGCHLD` + `waitpid()`, restarts the ones that die
unexpectedly (at most 3 times each), reports their status on `SIGUSR1`, and shuts
everything down gracefully on `SIGINT` / `SIGTERM`. Every significant event is written
to a timestamped log file.

## Requirements

- Linux (native or WSL on Windows). `fork`, `exec` and POSIX signals do not work in plain Windows.
- `gcc`, `make`, `bash`, `pgrep` (package `procps`)

## Build

```bash
make            # builds ./watchdog and ./worker
make clean      # removes build artifacts and logs
```

The build uses `-Wall -Wextra -Werror`, so it must compile without warnings.

## Run

```bash
./watchdog
```

The log is written to `watchdog.log` in the current directory.
In a second terminal, send signals to the watchdog PID (printed at start / `pgrep watchdog`):

| Action                    | Command                          | Effect |
|---------------------------|----------------------------------|--------|
| Status report             | `kill -SIGUSR1 <WATCHDOG_PID>`   | PID and state of every worker is written to the log |
| Graceful shutdown         | `kill -SIGTERM <WATCHDOG_PID>`   | SIGTERM to workers, wait, SIGKILL after timeout |
| Graceful shutdown (Ctrl+C)| `Ctrl+C` in the watchdog terminal| Same as SIGTERM |
| Simulate a crash (test)   | `kill -SIGKILL <WORKER_PID>`     | Worker is restarted (max 3 times) |

Always check that `<WORKER_PID>` really belongs to a `worker` process
(`ps -p <PID> -o comm=`) before killing it.

By default the watchdog starts 4 workers (IDs 1-4) that run until they are stopped.
For a quick demo of every behaviour without sending any signal, start it with
`WATCHDOG_DEMO=1 ./watchdog`: worker 3 then exits with code 0 after 3 s (normal exit,
not restarted) and worker 4 exits with code 1 after 1 s (restarted 3 times, then
`RESTART LIMIT`). A health check line (`Health check: N/4 workers running`) is written
every 5 s in both modes.

## Project structure

```
Makefile
README.md
include/   logger.h  signal_manager.h  process_manager.h
src/       main.c  signal_manager.c  process_manager.c  logger.c
workers/   worker.c              test worker (configurable lifetime / exit code)
tests/     test_scenarios.sh     automated test scenarios
           mock_watchdog.c       fake supervisor, only to debug the test script
docs/      TEST_SCENARIOS.md  REPORT.pdf
```

## Modules and responsibilities

| Module | Files | Owner |
|--------|-------|-------|
| Signal management and main loop | `signal_manager.*`, `main.c` | Student 1 |
| Child process management | `process_manager.*` | Student 2 |
| Logging and test infrastructure | `logger.*`, `Makefile`, `tests/`, `README.md`, `docs/TEST_SCENARIOS.md`, `workers/worker.c` | Student 3 |

Design rule: signal handlers only set `volatile sig_atomic_t` flags. All real work
(logging, `waitpid`, `fork`, status reports) happens in the main loop, because
`fprintf`, `malloc` and `fork` are not async-signal-safe.

## Log format

Every line: `[YYYY-MM-DD HH:MM:SS] message`

Messages used by the watchdog (the test script searches for these keywords, case-insensitive):

| Event | Example |
|-------|---------|
| Process started | `STARTED worker 1 pid=4242` |
| Normal exit | `EXITED NORMALLY worker 1 pid=4242 code=0` |
| Error exit | `TERMINATED worker 1 pid=4242 error exit code=1` |
| Killed by signal | `KILLED BY SIGNAL worker 1 pid=4242 signal=9` |
| Restart | `RESTART worker 1 attempt 2/3` |
| Restart limit | `RESTART LIMIT reached for worker 1, giving up` |
| Status report | `=== STATUS REPORT (SIGUSR1) ===` followed by one `worker N \| pid=... \| state=... \| restarts=...` line per worker |
| Shutdown | `SHUTDOWN initiated, sending SIGTERM to workers` ... `SHUTDOWN complete` |
| Shutdown timeout | `SHUTDOWN timeout, sending SIGKILL to remaining workers` (only if a worker ignores SIGTERM for 5 s) |
| Signal sent | `Sent signal 15 to worker 1 pid=4242` |
| Health check | `Health check: 4/4 workers running` (every 5 s) |
| Watchdog start | `watchdog started pid=4200` |

## Logger API (`include/logger.h`)

```c
int  logger_init(const char *path);
void logger_close(void);
void logger_log(const char *fmt, ...);                         /* timestamped line */
void logger_report_status(const worker_info_t *w, int count);  /* SIGUSR1 report   */
const char *logger_state_name(worker_state_t s);
```

Never call these from inside a signal handler.

## Tests

```bash
make test         # runs tests/test_scenarios.sh against ./watchdog
make test-mock    # same script against a fake supervisor (script debugging only)
```

The script covers all scenarios of the assignment (normal operation, normal termination,
unexpected termination, restart limit, status reporting, graceful shutdown with SIGTERM and
SIGINT, log verification). Logs of each run are saved in `tests/results/`.
Scenario descriptions and results: [docs/TEST_SCENARIOS.md](docs/TEST_SCENARIOS.md).

The Ctrl+C shutdown must also be verified manually: start `./watchdog`, press `Ctrl+C`,
then check that no `worker` process remains (`pgrep worker`) and the log contains the shutdown lines.

Result of the last run: `make test` -> 19 passed, 0 failed (`make test-mock` -> 19 passed, 0 failed).

## Sample log

Real run with `WATCHDOG_DEMO=1`, `kill -USR1` after ~6 s, then `kill -TERM`:

```
[2026-11-27 22:21:49] watchdog started pid=1983
[2026-11-27 22:21:49] STARTED worker 1 pid=1985
[2026-11-27 22:21:49] STARTED worker 2 pid=1986
[2026-11-27 22:21:49] STARTED worker 3 pid=1987
[2026-11-27 22:21:49] STARTED worker 4 pid=1988
[2026-11-27 22:21:50] TERMINATED worker 4 pid=1988 error exit code=1
[2026-11-27 22:21:50] RESTART worker 4 attempt 1/3
[2026-11-27 22:21:50] STARTED worker 4 pid=1989
[2026-11-27 22:21:51] TERMINATED worker 4 pid=1989 error exit code=1
[2026-11-27 22:21:51] RESTART worker 4 attempt 2/3
[2026-11-27 22:21:51] STARTED worker 4 pid=1990
[2026-11-27 22:21:52] EXITED NORMALLY worker 3 pid=1987 code=0
[2026-11-27 22:21:52] TERMINATED worker 4 pid=1990 error exit code=1
[2026-11-27 22:21:52] RESTART worker 4 attempt 3/3
[2026-11-27 22:21:52] STARTED worker 4 pid=1991
[2026-11-27 22:21:53] TERMINATED worker 4 pid=1991 error exit code=1
[2026-11-27 22:21:53] RESTART LIMIT reached for worker 4, giving up
[2026-11-27 22:21:54] Health check: 2/4 workers running
[2026-11-27 22:21:55] === STATUS REPORT (SIGUSR1) ===
[2026-11-27 22:21:55] worker 1 | pid=1985 | state=RUNNING | restarts=0
[2026-11-27 22:21:55] worker 2 | pid=1986 | state=RUNNING | restarts=0
[2026-11-27 22:21:55] worker 3 | pid=0 | state=EXITED_NORMAL | restarts=0
[2026-11-27 22:21:55] worker 4 | pid=0 | state=RESTART_LIMIT_REACHED | restarts=3
[2026-11-27 22:21:55] === END OF REPORT ===
[2026-11-27 22:21:56] SHUTDOWN initiated, sending SIGTERM to workers
[2026-11-27 22:21:56] Sent signal 15 to worker 1 pid=1985
[2026-11-27 22:21:56] Sent signal 15 to worker 2 pid=1986
[2026-11-27 22:21:56] KILLED BY SIGNAL worker 1 pid=1985 signal=15
[2026-11-27 22:21:56] KILLED BY SIGNAL worker 2 pid=1986 signal=15
[2026-11-27 22:21:56] SHUTDOWN complete
```

## Team

| Student | GitHub | Module |
|---------|--------|--------|
| Student 1 | ... | Signal management and main program |
| Student 2 | ... | Child process management |
| Student 3 | ... | Logging and test infrastructure |
