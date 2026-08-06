#ifndef COMMON_H
#define COMMON_H

// NOTE: host/main.c defines BASE_IMPLEMENTATION and STB_TRUETYPE_IMPLEMENTATION
// before including this header. lib/main.c does not -> declarations only.
#include "common/base.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include "host/platform.h"

#include "HandmadeMath.h"

#include "stb_truetype.h"

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

typedef struct {
  f32 x, y, w, h;
  f32 u_min_x, u_min_y;
  f32 u_max_x, u_max_y;
  f32 r, g, b, a;
} QuadInstance;
typedef struct {
  QuadInstance *data;
  u32 length;
  u32 capacity;
} QuadInstanceList;

typedef struct {
  f32 x, y, w, h;
} Rect;

typedef enum CaretAffinity {
  CARET_AFFINITY_UPSTREAM,
  CARET_AFFINITY_DOWNSTREAM
} CaretAffinity;
typedef struct Cursor {
  u32 offset;

  u32 prefered_col;
  bool prefered_col_valid;

  CaretAffinity affinity;
} Cursor;

typedef struct Atlas {
  // stbtt_fontinfo font;
  u8 *data;
  u32 w, h;

  stbtt_bakedchar glyphs[96]; // ASCII 32..126 is 95 glyphs

  f32 font_size, line_height;
  f32 ascent, descent;
  f32 advance;

  f32 white_x;
  f32 white_y;
} Atlas;

typedef Str PieceTable;

typedef struct Document {
  PieceTable table;
  u64 revision;
} Document;
typedef enum BreakKind {
  BREAK_NONE = 0,
  BREAK_SOFT,
  BREAK_HARD,
} BreakKind;
typedef struct LayoutRow {
  u32 offset_start;
  u32 offset_end;
  BreakKind break_kind;

  f32 y, w, h;
} LayoutRow;
typedef struct Layout {
  Arena *arena;
  LayoutRow *rows;
  u32 row_count;
  u32 row_capacity;

  float wrap_width;
  float glyph_advance;
  float line_height;

  float content_width;
  float content_height;

  u64 document_revision;
} Layout;
typedef struct Viewport {
  float x, y;
  float w, h;
  float scroll_x, scroll_y;
} Viewport;
typedef struct Editor {
  Document doc;
  Cursor cursor;
  Layout layout;
  Viewport vp;

  Vec4 color;
  bool wrap_enabled;
} Editor;

typedef struct Application {
  Editor editor;

  QuadInstanceList quad_list;
  bool editor_quad_is_dirty;

  u32 w, h;

  Platform plat;
  f64 fps;
  Atlas atlas;

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

  VkBuffer instance_buffer;
  VkDeviceMemory instance_memory;
  void *instance_mapped_array;

  VkBuffer *uniform_buffers;
  VkDeviceMemory *uniform_memories;
  void **uniform_mapped_arrays;

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

#define FIRST_CHAR 32
#define CHAR_COUNT 96

#define max_quad_count (1 << 14)

#endif // COMMON_H
