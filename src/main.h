#ifndef MAIN_H
#define MAIN_H

#define BASE_IMPLEMENTATION
#include "base.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "lib/math.h"

// #define STB_IMAGE_IMPLEMENTATION
// #include "lib/image.h"

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "lib/image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "lib/font.h"

#pragma GCC diagnostic pop

#define VKTRY(expr, msg)                                                       \
  do {                                                                         \
    VkResult vk_result__ = (expr);                                             \
    if (vk_result__ != VK_SUCCESS) {                                           \
      fprintf(stderr, "%s: VkResult=%d\n", msg, vk_result__);                  \
      return -1;                                                               \
    }                                                                          \
  } while (0)

#define MAX_FRAMES_IN_FLIGHT 2

typedef struct Viewport {
  i32 x, y;
  u32 w, h;
  u32 padding_h, padding_v;
} Viewport;
typedef struct Cursor {
  u32 col;
  u32 row;
} Cursor;

typedef struct Application {
  Str editor_text;
  u32 editor_glyph_count;
  Viewport editor_viewport;
  Cursor editor_cursor;

  // plat
  GLFWwindow *window;

  Arena *vulkan_arena;
  Arena *scratch_arena;

  VkInstance instance;
  VkSurfaceKHR surface;

  u32 inflight_count;
  u32 frame_index;

  VkPhysicalDevice physical_device;
  VkDevice device;

  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR swapchain_format;
  VkExtent2D swap_extent;
  u32 swapchain_images_count;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;

  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet *descriptor_sets;

  VkImage depth_image;
  VkDeviceMemory depth_memory;
  VkImageView depth_view;

  VkImage texture_image;
  VkDeviceMemory texture_memory;
  VkImageView texture_view;
  VkSampler texture_sampler;

  VkBuffer geometry_buffer;
  VkDeviceMemory geometry_memory;
  VkDeviceSize vertex_offset;
  VkDeviceSize index_offset;

  VkBuffer *uniform_buffers;
  VkDeviceMemory *uniform_memories;
  void **uniform_buffers_mapped;

  u32 graphic_queue_index;
  VkQueue graphic_queue;
  VkCommandPool graphic_command_pool;
  VkCommandBuffer *graphic_command_buffers;

  u32 transfer_queue_index;
  VkQueue transfer_queue;
  VkCommandPool transfer_command_pool;
  VkCommandBuffer transfer_command_buffer;

  VkSemaphore *image_available_semas;
  VkSemaphore *render_finish_semas;
  VkFence *draw_fences;
} Application;

typedef struct {
  // stbtt_fontinfo font;
  u8 *data;
  u32 w, h;

  stbtt_bakedchar glyphs[96]; // ASCII 32..126 is 95 glyphs

  f32 font_size, line_height;
  f32 ascent, descent;
} Atlas;

#define FIRST_CHAR 32
#define CHAR_COUNT 96

#endif
