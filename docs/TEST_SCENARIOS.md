# Test Scenarios

Run automatically with `make test` (script: `tests/test_scenarios.sh`).
Fill the **Result** column after running against the real watchdog.

| # | Scenario | Steps | Expected result | Log evidence | Result |
|---|----------|-------|-----------------|--------------|--------|
| 1 | Normal operation | Start `./watchdog` | At least 2 workers running; PIDs tracked | `STARTED worker N pid=...` | |
| 2 | Normal termination | Make one worker exit with code 0 (`kill -USR2 <WORKER_PID>` for the test worker) | Exit detected as normal; worker is **not** restarted | `EXITED NORMALLY ...` | |
| 3 | Unexpected termination | Verify PID is a `worker` (`ps -p PID -o comm=`), then `kill -SIGKILL <WORKER_PID>` | Death detected via SIGCHLD; worker restarted with a new PID | `KILLED BY SIGNAL ...`, `RESTART worker N attempt 1/3` | |
| 4 | Restart limit | Kill the same logical worker 4 times in a row | Restarted after kills 1-3; no restart after the 4th | `RESTART LIMIT reached ...` | |
| 5 | Status reporting | `kill -SIGUSR1 <WATCHDOG_PID>` | PID and state of every worker written to the log | `=== STATUS REPORT (SIGUSR1) ===` + one line per worker | |
| 6 | Graceful shutdown (SIGTERM) | `kill -SIGTERM <WATCHDOG_PID>` | Workers get SIGTERM, are reaped, watchdog exits; no zombies/orphans | `SHUTDOWN initiated`, `SHUTDOWN complete` | |
| 7 | Graceful shutdown (Ctrl+C) | Press Ctrl+C in the watchdog terminal (manual) + `SIGINT` in script | Same as scenario 6 | same as 6 | |
| 8 | Log verification | Inspect `watchdog.log` | Every significant event present, each line timestamped | `[YYYY-MM-DD HH:MM:SS] ...` | |

## Notes

- Scenario 2 relies on `workers/worker.c` handling `SIGUSR2` with `exit(0)`.
  If Student 2 uses a different worker, adapt this step.
- Only a `worker` process may be killed in tests. Always verify the PID first.
- Evidence (screenshots, `tests/results/*.log`) goes into `docs/REPORT.pdf`.
