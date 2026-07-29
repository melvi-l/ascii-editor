#include "main.h"

#include <vulkan/vulkan_core.h>

typedef enum {
  RENDER_COMMAND_RECT,
  RENDER_COMMAND_IMAGE,
  RENDER_COMMAND_TEXT,
} RenderCommandKind;

typedef struct {
  Vec4 color;
} RectRenderData;

typedef struct {
  // TextureHandle texture;
  Rect src;
  Vec4 tint;
} ImageRenderData;

typedef struct {
  Str str;
  f32 font_size;
  Vec4 color;
} TextRenderData;

typedef struct {
  RenderCommandKind kind;

  Rect boundingBox;
  // Rect clip;
  // f32 z;

  union {
    RectRenderData rect;
    ImageRenderData image;
    TextRenderData text;
  };
} RenderCommand;

typedef struct Vertex {
  f32 x, y;
  f32 u, v;
} Vertex;

typedef struct UniformBufferObject {
  Mat4 proj;
} UniformBufferObject;

static bool rd_create_instance(Application *app);
static bool rd_init(Application *app);
static void rd_cleanup(Application *app);
static bool rd_resize(Application *app);
bool rd_create_pipeline(Application *app);
bool rd_create_descriptor_set(Application *app);
bool rd_upload_bitmap(Application *app, u8 *bitmap, u32 width, u32 height);

bool swapchain_init(Arena *arena, Application *app);
bool swapchain_cleanup(Application *app);
bool depth_buffer_init(Application *app);
bool depth_buffer_cleanup(Application *app);

bool has_extension(u32 actual_count, VkExtensionProperties *actual_props,
                   const char *const expected);
bool get_extensions(Arena *arena, u32 *extension_count,
                    const char ***extension_names);
bool get_layers(Arena *arena, u32 *layer_count, const char ***layer_properties);

bool create_buffer(Application *app, VkDeviceSize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                   VkSharingMode sharing_mode, uint32_t queueFamilyIndexCount,
                   const uint32_t *pQueueFamilyIndices, VkBuffer *buffer,
                   VkDeviceMemory *memory);
bool upload_device_local_array(Application *app, void *array,
                               VkDeviceSize buffer_size,
                               VkBufferUsageFlags additional_usage,
                               VkBuffer *buffer, VkDeviceMemory *memory);

bool create_image(Application *app, u32 width, u32 height, VkFormat format,
                  VkMemoryPropertyFlags properties, VkImageTiling tiling,
                  VkImageUsageFlags usage, VkSharingMode sharing_mode,
                  u32 queue_family_index_count,
                  const u32 *p_queue_family_indices, VkImage *image,
                  VkDeviceMemory *memory);
bool copy_buffer_to_image(VkCommandBuffer cmd, u32 w, u32 h, VkBuffer buffer,
                          VkImage image);
void transition_image_layout(
    VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
    VkImageLayout new_layout, VkAccessFlags2 src_access_mask,
    VkAccessFlags2 dst_access_mask, VkPipelineStageFlags2 src_stage_mask,
    VkPipelineStageFlags2 dst_stage_mask, u32 src_queue_family_index,
    u32 dst_queue_family_index, VkImageAspectFlagBits image_aspect);

bool find_depth_format(Application *app, VkFormat *out);
