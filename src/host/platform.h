#ifndef PLATFORM_H
#define PLATFORM_H

#include "common/base.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef void (*PlatResizeCallback)(u32 width, u32 height, void *user_data);
typedef void (*PlatCharCallback)(u32 c, void *user_data);
typedef void (*PlatKeyCallback)(i32 key, i32 scancode, i32 action, i32 mods, void *user_data);

typedef enum {
  PLAT_MOUSE_MOVE,
  PLAT_MOUSE_BUTTON,
  PLAT_MOUSE_SCROLL,
  PLAT_MOUSE_ENTER,
} PlatMouseKind;

typedef struct {
  PlatMouseKind kind;
  union {
    struct { f64 x, y; }               move;
    struct { i32 button, action, mods; } button;
    struct { f64 xoff, yoff; }         scroll;
    struct { bool entered; }           enter;
  } u;
} PlatMouseEvent;

typedef void (*PlatMouseCallback)(const PlatMouseEvent *ev, void *user_data);

typedef GLFWwindow PlatWindow;
typedef struct {
  PlatWindow *window;

  PlatResizeCallback resize_callback;
  PlatCharCallback char_callback;
  PlatKeyCallback key_callback;
  PlatMouseCallback mouse_callback;
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

void plat_set_mouse_callback(Platform *plat, PlatMouseCallback callback);

#endif // PLATFORM_H
