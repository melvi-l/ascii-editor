#include "host/platform.h"
#include <GLFW/glfw3.h>

void plat_resize(GLFWwindow *window, int width, int height) {
  Platform *plat = glfwGetWindowUserPointer(window);
  assert(plat != NULL);

  printf("[PLAT] resize: (width=%4i; height=%4i)\n", width, height);

  if (plat->resize_callback != NULL) {
    plat->resize_callback((u32)width, (u32)height, plat->user_data);
  }
}

int plat_init(Platform *plat, PlatResizeCallback resize_callback,
              void *user_data) {
  assert(plat != NULL);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  plat->window = glfwCreateWindow(1000, 1000, "Vulkan", NULL, NULL);
  plat->resize_callback = resize_callback;
  plat->user_data = user_data;

  glfwSetWindowUserPointer(plat->window, plat);
  glfwSetFramebufferSizeCallback(plat->window, plat_resize);

  assert(plat->window);

  printf("[PLAT]: window created\n");
  return 0;
}

void plat_destroy(Platform *plat) {
  if (plat->window != NULL) {
    glfwDestroyWindow(plat->window);
    plat->window = NULL;
  }
  glfwTerminate();
}

void plat_get_window_size(Platform *plat, u32 *width, u32 *height) {
  int w, h;
  glfwGetFramebufferSize(plat->window, &w, &h);
  *width = (u32)w;
  *height = (u32)h;
}

VkResult plat_create_vulkan_surface(PlatWindow *window, VkInstance instance,
                                    const VkAllocationCallbacks *allocator,
                                    VkSurfaceKHR *surface) {
  assert(window != NULL);
  assert(instance != VK_NULL_HANDLE);
  assert(surface != NULL);

  VkResult result =
      glfwCreateWindowSurface(instance, window, allocator, surface);

  return result == VK_SUCCESS;
}

bool plat_should_close(Platform *plat) {
  return glfwWindowShouldClose(plat->window);
}

void plat_poll_events() { glfwPollEvents(); }

static void char_callback(GLFWwindow *window, u32 codepoint) {
  Platform *plat = glfwGetWindowUserPointer(window);
  assert(plat != NULL);

  // printf("[PLAT] char callback: (codepoint='%u')\n", codepoint);

  if (plat->char_callback != NULL) {
    plat->char_callback(codepoint, plat->user_data);
  }
}

static void key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action,
                         i32 mods) {
  Platform *plat = glfwGetWindowUserPointer(window);
  assert(plat != NULL);

  // printf("[PLAT] key callback: (key='%i', scancode=%i, action=%i,
  // mods=%i)\n",
  //        key, scancode, action, mods);

  if (plat->key_callback != NULL) {
    plat->key_callback(key, scancode, action, mods, plat->user_data);
  }
}

void plat_set_char_callback(Platform *plat, PlatCharCallback callback) {
  plat->char_callback = callback;
  glfwSetCharCallback(plat->window, char_callback);
}
void plat_set_key_callback(Platform *plat, PlatKeyCallback callback) {
  plat->key_callback = callback;
  glfwSetKeyCallback(plat->window, key_callback);
}
static void mouse_pos_cb(GLFWwindow *window, double x, double y) {
  Platform *plat = glfwGetWindowUserPointer(window);
  if (plat->mouse_callback != NULL) {
    PlatMouseEvent ev = { .kind = PLAT_MOUSE_MOVE, .u.move = { x, y } };
    plat->mouse_callback(&ev, plat->user_data);
  }
}

static void mouse_button_cb(GLFWwindow *window, int button, int action,
                            int mods) {
  Platform *plat = glfwGetWindowUserPointer(window);
  if (plat->mouse_callback != NULL) {
    PlatMouseEvent ev = { .kind = PLAT_MOUSE_BUTTON,
                          .u.button = { button, action, mods } };
    plat->mouse_callback(&ev, plat->user_data);
  }
}

static void mouse_scroll_cb(GLFWwindow *window, double xoff, double yoff) {
  Platform *plat = glfwGetWindowUserPointer(window);
  if (plat->mouse_callback != NULL) {
    PlatMouseEvent ev = { .kind = PLAT_MOUSE_SCROLL,
                          .u.scroll = { xoff, yoff } };
    plat->mouse_callback(&ev, plat->user_data);
  }
}

static void mouse_enter_cb(GLFWwindow *window, int entered) {
  Platform *plat = glfwGetWindowUserPointer(window);
  if (plat->mouse_callback != NULL) {
    PlatMouseEvent ev = { .kind = PLAT_MOUSE_ENTER,
                          .u.enter = { entered != 0 } };
    plat->mouse_callback(&ev, plat->user_data);
  }
}

void plat_set_mouse_callback(Platform *plat, PlatMouseCallback callback) {
  plat->mouse_callback = callback;
  glfwSetCursorPosCallback(plat->window, mouse_pos_cb);
  glfwSetCursorEnterCallback(plat->window, mouse_enter_cb);
  glfwSetMouseButtonCallback(plat->window, mouse_button_cb);
  glfwSetScrollCallback(plat->window, mouse_scroll_cb);
}

f64 plat_compute_fps() {
  static f64 start_time_ms = 0.0;
  static int frame_count = 0;

  f64 current_time_ms = now_milliseconds();

  if (start_time_ms == 0.0) {
    start_time_ms = current_time_ms;
  }

  frame_count++;

  f64 elapsed = current_time_ms - start_time_ms;

  f64 fps = ((f64)frame_count / elapsed) * 1000.;
  frame_count = 0;
  start_time_ms = current_time_ms;

  return fps;
}
