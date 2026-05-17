# Renderer In Practice

A hands-on course in building a renderer for a real product — no tutorials, just practice.

## Overview

This is the codebase for Renderer In Practice: a product-driven case study where you build a renderer grounded in a specific brief rather than following prescribed steps.

The project is split into two layers: an **engine** (compiled as `libengine.dylib`) that owns all rendering logic, and a **host** that loads it at runtime through a versioned C API. That boundary is where your work begins. You work on the engine side; the host stays stable.

Because the engine exposes a plain C API, any host that can load a shared library can drive it: a native macOS app, a headless worker, a Rust application, or a web frontend talking to a compiled engine binary. The current hosts are a macOS Metal window and a CLI loop, but the architecture doesn't prescribe what the host has to be.

You can also recompile and reload the engine while the host is running with no restart required.

## Prerequisites

This project requires Xcode 15 or later. Install it from the Mac App Store if you haven't already. Everything else is available via Homebrew:

```sh
brew install cmake ninja just
```

For the macOS GUI host, you also need `tuist` (`brew install tuist`). `xcbeautify` is optional but makes Xcode build output readable (`brew install xcbeautify`).

## Getting Started

Configure and build everything:

```sh
just build
```

All artifacts land in `build/`.

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
├── host/      # Harness layer — engine module loader, CLI and macOS hosts
├── third_party/    # Third-party dependencies (glm, metal-cpp)
└── justfile   # Task runner — prefer this over invoking cmake directly
```

## Key Concepts

**Harness vs. engine.**
The host is the harness: it owns the window, the run loop, and the dylib lifecycle. The engine owns all rendering. They share nothing except the structs in `core/include/engine/api.h`. This boundary is the central design constraint of the course.

**Versioned C ABI.**
`EngineAPI` is a struct of function pointers tagged with `abi_version` and `struct_size`. The host validates both before calling anything. Mismatches are caught and reported rather than crashing, which makes it safe to reload a dylib compiled against a different build.

**Hot reload mechanism.**
`DynamicLibrary::changed()` compares the dylib's mtime on every tick. When a change is detected, `EngineModule::reload()` calls `shutdown()` on the old API, unloads the dylib, reloads it, re-binds the function pointers, and calls `init()` again. The host process and its window never stop.

**Metal shader pipeline.**
Shaders are compiled by CMake via `xcrun metal` and `xcrun metallib` into a `.metallib` bundle. The path is baked in at compile time as `ENGINE_SHADER_LIB_PATH`. Changing a shader requires `just engine`, which triggers a full engine rebuild including shader recompilation.

## Configuration

| Variable | Type | Default | Description |
|---|---|---|---|
| `ENGINE_BACKEND` | CMake string | `Metal` | Renderer backend. Only `Metal` is supported. |
| `CMAKE_BUILD_TYPE` | CMake string | `Debug` | Standard CMake build type. |

Both are passed via the `backend` argument to just recipes, e.g. `just build Metal`.
