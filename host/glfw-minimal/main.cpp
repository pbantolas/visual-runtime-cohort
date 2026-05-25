#include "engine_module.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__linux__)
#include <GLFW/glfw3native.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {

struct AppState {
  EngineModule *engine = nullptr;
};

void glfw_error(int code, const char *description) {
  std::fprintf(stderr, "[glfw-minimal] GLFW error %d: %s\n", code,
               description ? description : "unknown");
}

void framebuffer_resized(GLFWwindow *window, int width, int height) {
  auto *state = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (!state || !state->engine) {
    return;
  }

  state->engine->resize(static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height));
  std::fprintf(stderr, "[glfw-minimal] resized to %dx%d\n", width, height);
}

bool attach_surface(GLFWwindow *window, EngineModule &engine) {
#if defined(__linux__)
  Display *display = glfwGetX11Display();
  if (!display) {
    std::fprintf(stderr,
                 "[glfw-minimal] failed to get XCB surface handles\n");
    return false;
  }

  Window x11_window = glfwGetX11Window(window);
  xcb_connection_t *connection = XGetXCBConnection(display);
  if (x11_window == 0 || !connection || xcb_connection_has_error(connection)) {
    std::fprintf(stderr,
                 "[glfw-minimal] failed to get XCB surface handles\n");
    return false;
  }

  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);

  SurfaceDescriptor surface{
      SurfaceKind::LinuxXcbWindow,
      connection,
      static_cast<uintptr_t>(x11_window),
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height),
  };
  std::fprintf(stderr,
               "[glfw-minimal] attaching LinuxXcbWindow surface (%ux%u)\n",
               surface.width, surface.height);
  engine.attachSurface(surface);
  return true;
#else
  (void)window;
  (void)engine;
  std::fprintf(stderr,
               "[glfw-minimal] native surface handles are not implemented for "
               "this platform\n");
  return false;
#endif
}

} // namespace

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  glfwSetErrorCallback(glfw_error);

  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Graphics Engine", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }

  EngineModule engine = EngineModule::open(ENGINE_LIB_PATH);
  if (!engine) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  AppState state{&engine};
  glfwSetWindowUserPointer(window, &state);
  glfwSetFramebufferSizeCallback(window, framebuffer_resized);

  if (!attach_surface(window, engine)) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  using clock = std::chrono::steady_clock;
  auto last = clock::now();

  while (!glfwWindowShouldClose(window)) {
    if (engine.reloadIfChanged()) {
      std::printf("[host] reloaded (frame %llu)\n", engine.frameCount());
    }

    auto now = clock::now();
    float dt = std::chrono::duration<float>(now - last).count();
    last = now;

    engine.tick(dt);
    glfwPollEvents();
  }

  std::printf("[glfw-minimal] exiting after %llu frames\n",
              engine.frameCount());
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
