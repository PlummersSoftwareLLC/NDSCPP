# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run

```shell
make            # build ndscpp (uses clang++, C++20)
make clean      # remove build artifacts
./ndscpp        # run (default port 7777, config file config.led)
./ndscpp -p 8080 -c myconfig.led
```

On first build, `config.led` and `secrets.h` are auto-created from their `.example` counterparts if they don't exist.

The web UI is served from the `webui/` directory at the same port as the API.

### Tests

```shell
make -C tests                                                        # build tests
LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib ./tests/tests      # run all tests
LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib ./tests/tests --gtest_filter=SuiteName.TestName
```

Tests require a running `ndscpp` instance (integration tests using GoogleTest + cpr, not unit tests).

### Monitor

```shell
make -C monitor   # build ledmon (ncurses terminal monitor)
```

## Architecture

All interfaces are defined in `interfaces.h`: `IController`, `ICanvas`, `ILEDFeature`, `ILEDGraphics`, `ISocketChannel`, `IEffectsManager`, `ILEDEffect`, `ISchedule`. Everything is coded to these interfaces.

**Object hierarchy:** `Controller` owns `ICanvas` objects → each `Canvas` owns `ILEDFeature` objects and an `EffectsManager` → each `LEDFeature` owns an `ISocketChannel`.

**Data flow:** `EffectsManager` runs an effect render loop on a background thread, writing pixels into `Canvas`'s pixel buffer. `LEDFeature::GetDataFrame()` slices its region of that buffer; `SocketChannel` queues and sends those frames over TCP to the ESP32's NightDriverStrip firmware on port 49152.

**HTTP server:** `WebServer` (crow-based) serves both the REST API (`/api/...`) and static webui assets from the `webui/` directory. REST handler logic lives in `apihelpers.h/.cpp` under the `ndscpp::api` namespace. `ApiRequestContext` bundles controller, mutex, filename, and request for handlers.

**Config:** `config.led` is JSON, loaded via `Controller::CreateFromFile()` → `from_json`. All classes implement `to_json`/`from_json` inline in their header files. `Controller::WriteToFile()` persists state; API handlers call `ctx.Persist()` after mutations (skipped if `?nopersist` query param is set).

**Effects:** Defined in `effects/`, inherit from `LEDEffectBase` (`ledeffectbase.h`). Add a new effect by subclassing, implementing `Start()` and `Update()`, then registering it in the effects catalog in `apihelpers.cpp`.

**Nearly header-only:** Only `main.cpp` and `apihelpers.cpp` are `.cpp` files. All class implementations live in `.h` files. Static ID counters (`_nextId`) for `Canvas`, `LEDFeature`, and `SocketChannel` are initialized in `main.cpp`. `apihelpers.cpp` exists solely to contain `BuildEffectCatalog()`, which includes all effect headers — keeping those includes out of `apihelpers.h` so they don't propagate to every includer. All actual API handler functions are `inline` in `apihelpers.h`.

**Vendored libraries:** `crow_all.h` (HTTP framework) and `json.hpp` (nlohmann/json) are single-header vendored dependencies.

**Global logger:** `spdlog` logger declared `extern` in `global.h`, defined in `main.cpp`. `secrets.h` (gitignored) holds any local secrets; `secrets.h.example` is the template.
