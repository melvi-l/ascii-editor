#ifndef PLAT_H
#define PLAT_H

#include "base.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef void (*PlatResizeCallback)(u32 width, u32 height, void *user_data);
typedef void (*PlatCharCallback)(u32 c, void *user_data);
typedef void (*PlatKeyCallback)(i32 key, i32 scancode, i32 action, i32 mods, void *user_data);
typedef GLFWwindow PlatWindow;
typedef struct {
  PlatWindow *window;

  PlatResizeCallback resize_callback;
  PlatCharCallback char_callback;
  PlatKeyCallback key_callback;
  void *user_data;
} Platform;

int plat_init(Platform *plat, PlatResizeCallback resize_callback,
              void *user_data);
void plat_resize(GLFWwindow *window, int width, int height);

void plat_destroy(Platform *plat);

void plat_get_window_size(Platform *plat, u32 *width, u32 *height);

VkResult plat_create_vulkan_surface(PlatWindow *window, VkInstance instance,
                                    const VkAllocationCallbacks *allocator,
                                    VkSurfaceKHR *surface);

bool plat_should_close(Platform *plat);

void plat_poll_events();

f64 plat_compute_fps();

#endif
