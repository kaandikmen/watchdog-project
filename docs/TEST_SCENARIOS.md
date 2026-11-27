# Test Scenarios

Run automatically with `make test` (script: `tests/test_scenarios.sh`).
Result column filled from the run of `make test` against the real watchdog (19 checks passed, 0 failed; 2026-11-27).

| # | Scenario | Steps | Expected result | Log evidence | Result |
|---|----------|-------|-----------------|--------------|--------|
| 1 | Normal operation | Start `./watchdog` | At least 2 workers running; PIDs tracked | `STARTED worker N pid=...` | PASS |
| 2 | Normal termination | Make one worker exit with code 0 (`kill -USR2 <WORKER_PID>` for the test worker) | Exit detected as normal; worker is **not** restarted | `EXITED NORMALLY ...` | PASS |
| 3 | Unexpected termination | Verify PID is a `worker` (`ps -p PID -o comm=`), then `kill -SIGKILL <WORKER_PID>` | Death detected via SIGCHLD; worker restarted with a new PID | `KILLED BY SIGNAL ...`, `RESTART worker N attempt 1/3` | PASS |
| 4 | Restart limit | Kill the same logical worker 4 times in a row | Restarted after kills 1-3; no restart after the 4th | `RESTART LIMIT reached ...` | PASS |
| 5 | Status reporting | `kill -SIGUSR1 <WATCHDOG_PID>` | PID and state of every worker written to the log | `=== STATUS REPORT (SIGUSR1) ===` + one line per worker | PASS |
| 6 | Graceful shutdown (SIGTERM) | `kill -SIGTERM <WATCHDOG_PID>` | Workers get SIGTERM, are reaped, watchdog exits; no zombies/orphans | `SHUTDOWN initiated`, `SHUTDOWN complete` | PASS |
| 7 | Graceful shutdown (Ctrl+C) | Press Ctrl+C in the watchdog terminal (manual) + `SIGINT` in script | Same as scenario 6 | same as 6 | PASS (SIGINT via script); manual Ctrl+C screenshot still to be added |
| 8 | Log verification | Inspect `watchdog.log` | Every significant event present, each line timestamped | `[YYYY-MM-DD HH:MM:SS] ...` | PASS |

## Notes

- Scenario 2 relies on `workers/worker.c` handling `SIGUSR2` with `exit(0)`.
  If Student 2 uses a different worker, adapt this step.
- The default worker set is 4 persistent workers (IDs 1-4). `WATCHDOG_DEMO=1` makes worker 3 exit
  normally after 3 s and worker 4 fail repeatedly, which is useful for manual demos but not for
  `make test`.
- Scenario 3: `KILLED BY SIGNAL worker N pid=... signal=9` followed by `RESTART worker N attempt 1/3`.
- Only a `worker` process may be killed in tests. Always verify the PID first.
- Evidence (screenshots, `tests/results/*.log`) goes into `docs/REPORT.pdf`.
