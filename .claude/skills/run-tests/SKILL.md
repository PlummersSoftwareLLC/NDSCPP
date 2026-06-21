---
name: run-tests
description: This skill should be used when the user asks to "run the tests", "build and test", "run ndscpp tests", "execute the test suite", "check if tests pass", or wants to verify that the application builds correctly and the test suite passes. Handles dependency verification, building ndscpp and the test suite, running the server in the background, and executing the tests.
version: 1.0.0
---

# Run NDSCPP Tests

Build the application, start it in the background, run the full test suite, and report the outcome.

## Process

### 1. Verify dependencies

Run the dependency check script from the project root:

```bash
bash .claude/skills/run-tests/scripts/check-deps.sh
```

If any dependencies are missing, the script prints exactly what to install and exits non-zero. Stop and show the user the output — do not proceed until dependencies are satisfied.

### 2. Build the main application

```bash
make
```

If the build fails, show the compiler errors and stop.

### 3. Build the test suite

```bash
make -C tests
```

If this fails, show the errors and stop.

### 4. Start ndscpp in the background

```bash
./ndscpp &
NDSCPP_PID=$!
```

Wait up to 5 seconds for the server to become ready by polling its health endpoint:

```bash
for i in $(seq 1 10); do
    curl -sf http://localhost:7777/api/controller >/dev/null 2>&1 && break
    sleep 0.5
done
```

If the server hasn't responded after 5 seconds, kill the process, show its output, and stop.

### 5. Run the tests

On Linux:
```bash
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/usr/local/lib" ./tests/tests
TEST_EXIT=$?
```

On macOS:
```bash
./tests/tests
TEST_EXIT=$?
```

### 6. Stop the server

```bash
kill $NDSCPP_PID 2>/dev/null
wait $NDSCPP_PID 2>/dev/null
```

Always kill the server, even if the tests failed.

### 7. Report

Exit with `$TEST_EXIT`. Report pass/fail and any failing test names from the GoogleTest output.

## Notes

- The default server port is `7777`. If the config file specifies a different port, adjust the health-check URL accordingly.
- `config.led` and `secrets.h` are auto-created from their `.example` counterparts on the first `make` run — this is expected and not an error.
- On Ubuntu, `cpr` has no apt package and must be built from source (the dependency check script explains how). Once installed, its shared library is at `/usr/local/lib/libcpr.so`, which is why `LD_LIBRARY_PATH` must include `/usr/local/lib`.
- To run a single test: append `--gtest_filter=SuiteName.TestName` to the test binary invocation.
