# Graphics Engine Cohort Harness

This context describes the language for the cohort harness and rendering engine boundary.

## Language

**Surface**:
The native drawing destination offered by the harness to the engine. A surface may wrap platform-specific windowing objects, but the engine boundary should name it as a surface rather than as a specific graphics API object.
_Avoid_: metal layer, drawable, window

**Platform Surface Handle**:
The platform-native object carried inside a surface so the rendering engine can bind its compiled renderer backend to the harness-owned drawing destination.
_Avoid_: graphics context, renderer-owned window

**Harness**:
The application shell that owns the process, window, run loop, and engine dylib lifecycle.
_Avoid_: app layer, host app

**Rendering Engine**:
The reloadable dylib that owns rendering behavior behind the engine API boundary.
_Avoid_: renderer app, host

**Engine API Boundary**:
The narrow C-compatible contract shared by the harness and the rendering engine.
_Avoid_: renderer API, Metal API

**Renderer Backend**:
The graphics API implementation compiled into the rendering engine for a given build. Renderer backend choice is a build-time concern, not a harness runtime concern.
_Avoid_: runtime renderer mode, host backend

**Backend Partition**:
A backend-named area that contains graphics API-specific renderer code or shader assets. Shared engine entry points should stay outside backend partitions.
_Avoid_: mixed renderer file, flat shader bucket

## Example Dialogue

Dev: "The harness has a surface ready for the rendering engine."

Domain expert: "Good. The surface may be backed by a platform object, but the engine API boundary should not call it a Metal layer."

Dev: "Should the harness pass a Vulkan instance?"

Domain expert: "No. The harness passes a platform surface handle; the rendering engine owns graphics API objects."

Dev: "Should the harness choose Vulkan or Metal?"

Domain expert: "No. The rendering engine is built with one renderer backend, and the harness only loads the engine."

Dev: "Where should the Vulkan shaders go?"

Domain expert: "In the Vulkan backend partition. Metal and Vulkan assets should not be mixed in one flat shader directory."
