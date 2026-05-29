#pragma once
#include <cstdint>

struct VisualRuntimeState {
  uint64_t frame_count;
  float elapsed_time;
};

enum class SurfaceKind : uint32_t {
  None = 0,
  MacOSMetalLayer = 1,
  LinuxXcbWindow = 2,
  LinuxWaylandSurface = 3,
};

// Describes the native rendering surface passed to the visual runtime on init.
// Handle fields are interpreted according to kind; null/zero for headless
// hosts.
struct SurfaceDescriptor {
  SurfaceKind kind;
  void *display_handle;
  uintptr_t surface_handle;
  uint32_t width;
  uint32_t height;
};

constexpr uint32_t VISUAL_RUNTIME_API_VERSION = 4;

struct VisualRuntimeAPI {
  uint32_t abi_version;
  uint32_t struct_size;
  const char *backend_name;

  void (*init)(VisualRuntimeState *, SurfaceDescriptor *);
  void (*resize)(VisualRuntimeState *, uint32_t, uint32_t);
  void (*update)(VisualRuntimeState *, float);
  void (*shutdown)(VisualRuntimeState *);
};

extern "C" {
const VisualRuntimeAPI *visual_runtime_get_api();
}
