# engine

A Metal-backed graphics engine built as a hot-reloadable shared library, with a thin host that drives it.

## Overview

The project is split into two layers: an **engine** (compiled as `libengine.dylib`) that owns all rendering logic, and a **host** that loads it at runtime and calls into it through a versioned C API. Because the engine is a separate dylib, you can recompile it while a host is running and the host will pick up the new code automatically — no restart required. The current renderer draws a triangle via Metal and is intentionally minimal: the point is the architecture, not the scene.

## Prerequisites

- macOS 14 or later
- Xcode 15 or later (provides `xcrun`, `metal`, `metallib`)
- CMake 3.25 or later
- Ninja (`brew install ninja`)
- just (`brew install just`)
- For the macOS GUI host only: Tuist (`brew install tuist`) and optionally xcbeautify (`brew install xcbeautify`)

## Getting Started

Configure and build everything:

```sh
just build
```

That configures CMake with Ninja, compiles the engine dylib, compiles and links the Metal shader library, and builds the CLI host binary. All artifacts land in `build/`.

To generate IDE tooling (clangd, etc.):

```sh
just compile-commands
```

### macOS GUI host

Generate the Xcode project and open it:

```sh
just macos-open
```

Or build and launch from the command line:

```sh
just macos-run
```

## Usage

### CLI host

Run the engine headlessly. The host ticks the engine twice per second and prints frame info to stdout:

```sh
just run
```

**Hot reload** — in a second terminal, rebuild only the engine dylib:

```sh
just engine
```

The running host detects the changed file and reloads it mid-session. You'll see `[host] reloaded (frame N)` in the first terminal. There's a `VERSION` string in `core/src/engine.cpp` you can change to confirm the reload is live.

### macOS GUI host

```sh
just macos-run
```

Opens a native Metal window. Hot reload works the same way: run `just engine` in a separate terminal while the app is running.

## Project Structure

```
engine/
├── core/      # Engine shared library (libengine.dylib) — renderer, shaders, public API
├── host/      # Harness layer — dylib loader, runtime, CLI and macOS hosts
├── Vendor/    # Third-party dependencies (glm, metal-cpp)
└── justfile   # Task runner — prefer this over invoking cmake directly
```

## Key Concepts

**Harness vs. engine.** The host is the harness: it owns the window, the run loop, and the dylib lifecycle. The engine owns all rendering. They share nothing except the structs in `core/include/engine/api.h`. This boundary is the central design constraint of the course.

**Versioned C ABI.** `EngineAPI` is a struct of function pointers tagged with `abi_version` and `struct_size`. The host validates both before calling anything. This makes it safe to reload a dylib compiled against a different build without crashing — mismatches are caught and reported.

**Hot reload mechanism.** `DynamicLibrary::changed()` compares the dylib's mtime on every tick. When a change is detected, `Runtime::reload()` calls `shutdown()` on the old API, unloads the dylib, reloads it, re-binds the function pointers, and calls `init()` again. The host process and its window never stop.

**Metal shader pipeline.** Shaders are compiled by CMake via `xcrun metal` and `xcrun metallib` into a `.metallib` bundle. The path is baked in at compile time as `ENGINE_SHADER_LIB_PATH`. Changing a shader requires a `just reload` (which triggers a full engine rebuild including shader recompilation).

## Configuration

| Variable | Type | Default | Description |
|---|---|---|---|
| `ENGINE_BACKEND` | CMake string | `Metal` | Renderer backend. Only `Metal` is supported. |
| `CMAKE_BUILD_TYPE` | CMake string | `Debug` | Standard CMake build type. |

Both are passed via the `backend` argument to just recipes, e.g. `just build Metal`.
