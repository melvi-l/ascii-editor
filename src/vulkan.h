#include "main.h"

#include <vulkan/vulkan_core.h>

typedef struct TextVertex {
  Vec2 position;
  Vec3 color;
  Vec2 uv;
} TextVertex;
typedef struct UniformBufferObject {
  Mat4 proj;
} UniformBufferObject;

static int vk_init(Application *app);
static void vk_cleanup(Application *app);
static int vk_resize(Application *app);
int vk_create_pipeline(Application *app);
int vk_create_descriptor_set(Application *app);
int vk_upload_bitmap(Application *app, u8 *bitmap, u32 width, u32 height);

int swapchain_init(Arena *arena, Application *app);
int swapchain_cleanup(Application *app);
int depth_buffer_init(Application *app);
int depth_buffer_cleanup(Application *app);

bool has_extension(u32 actual_count, VkExtensionProperties *actual_props,
                   const char *const expected);
bool get_extensions(Arena *arena, u32 *extension_count,
                    const char ***extension_names);
bool get_layers(Arena *arena, u32 *layer_count, const char ***layer_properties);

int create_buffer(Application *app, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkSharingMode sharing_mode,
                  uint32_t queueFamilyIndexCount,
                  const uint32_t *pQueueFamilyIndices, VkBuffer *buffer,
                  VkDeviceMemory *memory);
int upload_device_local_array(Application *app, void *array, VkDeviceSize buffer_size,
                 VkBufferUsageFlags additional_usage, VkBuffer *buffer,
                 VkDeviceMemory *memory);

int create_image(Application *app, u32 width, u32 height, VkFormat format,
                 VkMemoryPropertyFlags properties, VkImageTiling tiling,
                 VkImageUsageFlags usage, VkSharingMode sharing_mode,
                 u32 queue_family_index_count,
                 const u32 *p_queue_family_indices, VkImage *image,
                 VkDeviceMemory *memory);
int copy_buffer_to_image(VkCommandBuffer cmd, u32 w, u32 h, VkBuffer buffer,
                         VkImage image);
void transition_image_layout(
    VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
    VkImageLayout new_layout, VkAccessFlags2 src_access_mask,
    VkAccessFlags2 dst_access_mask, VkPipelineStageFlags2 src_stage_mask,
    VkPipelineStageFlags2 dst_stage_mask, u32 src_queue_family_index,
    u32 dst_queue_family_index, VkImageAspectFlagBits image_aspect);

bool find_depth_format(Application *app, VkFormat *out);
