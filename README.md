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
| Shutdown | `SHUTDOWN initiated ...` and `SHUTDOWN complete` |

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

## Sample log

```
(paste output of watchdog.log from a real run here)
```

## Team

| Student | GitHub | Module |
|---------|--------|--------|
| Student 1 | ... | Signal management and main program |
| Student 2 | ... | Child process management |
| Student 3 | ... | Logging and test infrastructure |
