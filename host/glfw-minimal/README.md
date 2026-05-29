# GLFW Minimal Harness

This is the minimal Linux harness for the visual runtime. It creates a GLFW
window, passes the native Linux surface handles through the Engine API Boundary,
and leaves Vulkan instance/device/surface ownership inside the visual runtime.

## Required Dependencies

Install development packages for:

- `just`, CMake, and Ninja
- a C++17 compiler
- GLFW with native X11 and Wayland support
- Vulkan headers and loader
- `glslangValidator`
- X11 development headers and libraries
- X11-XCB and XCB development headers and libraries
- Wayland client development headers and libraries

Package names differ by distribution. Known package sets:

Arch Linux:

```bash
sudo pacman -S just cmake ninja gcc glfw vulkan-headers vulkan-icd-loader \
  glslang libx11 libxcb wayland
```

Arch also needs a Vulkan driver for your GPU, such as `vulkan-intel`,
`vulkan-radeon`, or the appropriate NVIDIA Vulkan package. Current Arch uses a
unified `glfw` package. If your install still offers split GLFW packages, use
the Wayland-capable variant for a Wayland session.

Debian/Ubuntu:

```bash
sudo apt install cmake ninja-build g++ libglfw3-dev libvulkan-dev vulkan-tools \
  glslang-tools libx11-dev libx11-xcb-dev libxcb1-dev libwayland-dev
```

Some Debian/Ubuntu releases do not package `just` in the default apt
repositories. Install `just` separately, or run the equivalent CMake commands
directly.

## Build And Run

From the repo root:

```bash
just glfw-build
just glfw-run
```

While the harness is running, rebuild the visual runtime from another terminal:

```bash
just engine Vulkan
```
