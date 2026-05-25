# Graphics Engine Cohort Harness

This context describes the language for the cohort harness and visual runtime boundary.

## Language

**Surface**:
The native drawing destination offered by the harness to the engine. A surface may wrap platform-specific windowing objects, but the engine boundary should name it as a surface rather than as a specific graphics API object.
_Avoid_: metal layer, drawable, window

**Platform Surface Handle**:
The platform-native object carried inside a surface so the rendering engine can bind its compiled renderer backend to the harness-owned drawing destination.
_Avoid_: graphics context, renderer-owned window

**Linux Surface Variant**:
An explicit Linux platform surface shape used by the Linux harness when offering a drawing destination to the visual runtime. Wayland and X11 are separate variants rather than a single vague Linux window concept.
_Avoid_: generic Linux native window, universal desktop surface

**Linux XCB Window Surface**:
The first implemented Linux surface variant. The harness passes an XCB connection and XCB window identifier for an X11 window so the visual runtime's Vulkan backend can create the Vulkan surface.
_Avoid_: generic X11 handle, Xlib surface, GLFW-created Vulkan surface

**Linux Wayland Surface**:
A Linux surface variant for native Wayland sessions. The harness passes a Wayland display and Wayland surface so the visual runtime's Vulkan backend can create the Vulkan surface.
_Avoid_: Wayland mode, GLFW-created Vulkan surface, generic Linux window

**Harness**:
The application shell that owns the process, window, run loop, and engine dylib lifecycle.
_Avoid_: app layer, host app

**Linux Harness**:
A harness for modern Linux desktop environments that loads the same visual runtime boundary as the macOS harness while owning Linux-specific window and event-loop concerns.
_Avoid_: portable runtime window layer, Linux renderer

**GLFW Minimal Harness**:
The first Linux-oriented harness, located under `host/glfw-minimal`, that uses GLFW only to provide a window, event loop, resize notifications, and platform surface handles for the visual runtime. It is not a cross-platform UI framework or a replacement for the macOS harness.
_Avoid_: portable app shell, GLFW renderer, UI parity harness

**Visual Runtime**:
The reloadable dylib that owns visual behavior behind the engine API boundary. Use this as the student-facing top-level concept instead of the generic term engine when discussing rendering behavior.
_Avoid_: renderer app, host, generic engine

**Engine API Boundary**:
The narrow C-compatible contract shared by the harness and the visual runtime.
_Avoid_: renderer API, Metal API

**Renderer Backend**:
The graphics API implementation compiled into the rendering engine for a given build. Renderer backend choice is a build-time concern, not a harness runtime concern.
_Avoid_: runtime renderer mode, host backend

**Visual Runtime API**:
The product-shaped interface of the visual runtime for creating and updating visual state. It uses product terminology rather than generic renderer concepts unless the product itself needs those concepts. Callers use the API without knowing whether or how the runtime retains visual state internally. In the current harness, this API may begin as an internal runtime seam before being exposed across the engine API boundary.
_Avoid_: renderer API, RHI, backend API, premature mesh/material/object API

**Visual Runtime Feature Parity**:
The expectation that a student can work on the same visual-runtime behavior across supported harnesses, even when each harness has different native UI affordances.
_Avoid_: harness UI parity, identical app shell

**Render World**:
The visual runtime's internal retained visual state derived from Visual Runtime API calls and consumed by renderer backends each frame. The render world is not exposed to the harness as a data structure and should not force product-facing concepts before product features require them.
_Avoid_: product scene, harness world, backend state

**Frame Config**:
Product-shaped frame-level visual settings used by renderer backends, such as clear color or viewport-sized frame inputs. Frame config can be shared before richer product concepts exist because it describes frame intent rather than generic GPU resources. Its expected home is the runtime partition, not a renderer backend partition.
_Avoid_: pipeline config, render pass descriptor, backend settings

**Runtime Partition**:
The shared visual-runtime area for product-shaped runtime concepts, such as frame config, that are not specific to Metal or Vulkan. Runtime partition code should not become a graphics abstraction layer.
_Avoid_: RHI folder, shared backend layer, generic engine layer

**Backend Partition**:
A backend-named area that contains graphics API-specific renderer code or shader assets. Shared engine entry points should stay outside backend partitions.
_Avoid_: mixed renderer file, flat shader bucket

## Example Dialogue

Dev: "The harness has a surface ready for the visual runtime."

Domain expert: "Good. The surface may be backed by a platform object, but the engine API boundary should not call it a Metal layer."

Dev: "Should the harness pass a Vulkan instance?"

Domain expert: "No. The harness passes a platform surface handle; the visual runtime owns graphics API objects."

Dev: "Should the harness choose Vulkan or Metal?"

Domain expert: "No. The visual runtime is built with one renderer backend, and the harness only loads the runtime."

Dev: "Where should the Vulkan shaders go?"

Domain expert: "In the Vulkan backend partition. Metal and Vulkan assets should not be mixed in one flat shader directory."

Dev: "Where should clear color live if both Metal and Vulkan need it?"

Domain expert: "If it expresses product-shaped frame intent, put it in the runtime partition as frame config. Do not create a generic renderer abstraction just to deduplicate backend code."

Dev: "Should the Linux harness match the macOS harness UI?"

Domain expert: "No. The first Linux target is visual runtime feature parity, not identical native harness UI."

Dev: "Can the Linux harness pass one generic Linux window handle?"

Domain expert: "No. The surface should name the Linux surface variant explicitly, such as Wayland or X11, so the visual runtime can bind the correct Vulkan surface path."
