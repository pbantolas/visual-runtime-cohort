# Graphics Mentoring Cohort Project

## Overview

This is the codebase for the Graphics Mentoring Cohort: a product-driven case study where you build a renderer grounded in a specific use case rather than following prescribed steps.

The project is split into two layers: a **visual runtime** that owns all rendering & interaction logic, and a **host** that owns the app shell and loads the visual runtime at runtime. You work on the visual runtime side for the most part; harness changes will usually be provided as the product brief evolves.

The current hosts are a native macOS Metal window, a minimal Linux GLFW/Vulkan window, and a small CLI loop, but the architecture keeps rendering code out of the app target so the visual runtime can evolve independently.

You can also recompile and reload the visual runtime while the host is running with no restart required.

### Architecture at a glance

The diagram below shows the main pieces you will see in the codebase.

![macOS harness and visual runtime architecture](docs/architecture-diagram.png)

## Prerequisites

### macOS

This project requires Xcode 15 or later. Install it from the Mac App Store if you haven't already. Everything else is available via Homebrew:

```sh
brew install cmake ninja just
```

If you want to modify the macOS project itself, you may also need `tuist` (`brew install tuist`) to generate the Xcode project. `xcbeautify` is optional but makes Xcode build output readable (`brew install xcbeautify`).

### Linux

Linux users should use the Vulkan backend and the minimal GLFW harness. Follow the dependency and build instructions in [`host/glfw-minimal/README.md`](host/glfw-minimal/README.md).

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

Open the Xcode project:

```sh
just macos-open
```

Or build and launch from the command line:

```sh
just macos-run
```

### Linux GLFW host

Build and run the minimal GLFW harness:

```sh
just glfw-build
just glfw-run
```

Hot reload works the same way: run `just visual-runtime Vulkan` in a separate terminal while the app is running.

## Usage

### CLI host

Run the visual runtime headlessly. The host ticks the visual runtime twice per second and prints frame info to stdout:

```sh
just run
```

**Hot reload** — in a second terminal, rebuild only the visual runtime dylib:

```sh
just visual-runtime
```

The running host detects the changed file and reloads it mid-session. You'll see `[host] reloaded (frame N)` in the first terminal. There's a `VERSION` string in `core/src/visual_runtime.cpp` you can change to confirm the reload is live.

### macOS GUI host

```sh
just macos-run
```

Opens a native Metal window. Hot reload works the same way: run `just visual-runtime` in a separate terminal while the app is running.

## Project Structure

```
visual-runtime/
├── core/      # Visual runtime shared library (libvisual_runtime.dylib) — renderer, shaders, public API
├── host/      # Harness layer — visual runtime module loader, CLI, macOS, and GLFW hosts
├── third_party/    # Third-party dependencies (glm, metal-cpp)
└── justfile   # Task runner — prefer this over invoking cmake directly
```

## Key Concepts

**Harness vs. visual runtime.**
The host is the harness: it owns the window, the run loop, and the dylib lifecycle. The visual runtime owns all rendering. They share nothing except the structs in `core/include/visual_runtime/api.h`. This boundary is the central design constraint of the course.

**Versioned visual runtime ABI.**
`visual_runtime_get_api` is the only exported C-linkage entry point. It returns a `VisualRuntimeAPI` struct of function pointers tagged with `abi_version` and `struct_size`. The host validates both before calling anything. Mismatches are caught and reported rather than crashing, which makes it safe to reload a dylib compiled against a different build.

**Hot reload mechanism.**
`DynamicLibrary::changed()` compares the dylib's mtime on every tick. When a change is detected, `VisualRuntimeModule::reload()` calls `shutdown()` on the old API, unloads the dylib, reloads it, re-binds the function pointers, and calls `init()` again. The host process and its window never stop.

**Metal shader pipeline.**
Shaders are compiled by CMake via `xcrun metal` and `xcrun metallib` into a `.metallib` bundle. The path is baked in at compile time as `VISUAL_RUNTIME_SHADER_LIB_PATH`. Changing a shader requires `just visual-runtime`, which triggers a full visual runtime rebuild including shader recompilation.

## Configuration

| Variable | Type | Default | Description |
|---|---|---|---|
| `VISUAL_RUNTIME_BACKEND` | CMake string | `Metal` | Renderer backend. Supported values: `Metal`, `Vulkan`. |
| `CMAKE_BUILD_TYPE` | CMake string | `Debug` | Standard CMake build type. |

`VISUAL_RUNTIME_BACKEND` is passed via the `backend` argument to just recipes, e.g. `just build Metal`. `CMAKE_BUILD_TYPE` is currently fixed to `Debug` by the configure recipe.
