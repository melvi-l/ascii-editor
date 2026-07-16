#include "main.h"
#include <stdio.h>

static u32 vc = 0;
static u32 ic = 0;

#include "vulkan.c"

#define TV(x, y, r, g, b, u, v)                                                \
  ((TextVertex){.position = {.X = x, .Y = y},                                  \
                .color = {.R = r, .G = g, .B = b},                             \
                .uv = {.U = u, .V = v}})

#define BTV(x, y, u, v) TV(x, y, 1, 1, 1, u, v)

static void glfw_resize(GLFWwindow *window, int width, int height) {

  printf("GLFW Resize (width=%4i; height=%4i)\n", width, height);
  f64 start_time = now_seconds();
  Application *app = glfwGetWindowUserPointer(window);

  vk_resize(app);

  f64 end_time = now_seconds();
  f64 elapsed = end_time - start_time;
  printf("resizing take %.2fms.\n", elapsed * 1000);
}

static int glfw_init(Application *app) {
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  app->window = glfwCreateWindow(1000, 1000, "Vulkan", NULL, NULL);

  glfwSetWindowUserPointer(app->window, app);
  glfwSetFramebufferSizeCallback(app->window, glfw_resize);

  if (app->window == NULL) {
    fprintf(stderr, "Failed to create GLFW window\n");
    return -1;
  }

  printf("GLFW window created\n");
  int win_w, win_h;
  int fb_w, fb_h;

  glfwGetWindowSize(app->window, &win_w, &win_h);
  glfwGetFramebufferSize(app->window, &fb_w, &fb_h);

  printf("window size: %d x %d\n", win_w, win_h);
  printf("framebuffer size: %d x %d\n", fb_w, fb_h);

  return 0;
}

int init_atlas(Arena *arena, Str font_path, Atlas *a);

// @init

int draw_frame(Application *app) {
  VkCommandBuffer current_command_buffer = NULL;
  i32 image_index = vk_begin_rendering(app, &current_command_buffer);
  if (image_index < 0)
    perror("failed to begin rendering");

  vkCmdBindPipeline(current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    app->pipeline);
  VkBuffer vertex_buffers[] = {app->geometry_buffer};
  VkDeviceSize offsets[] = {app->vertex_offset};
  vkCmdBindVertexBuffers(current_command_buffer, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(current_command_buffer, app->geometry_buffer,
                       app->index_offset, VK_INDEX_TYPE_UINT16);

  // uniform
  {
    UniformBufferObject ubo = {};
    ubo.proj = Orthographic_RH_ZO(0.f, (f32)app->swap_extent.width, 0.f,
                                  (f32)app->swap_extent.height, -1.f, 1.f);

    memcpy(app->uniform_buffers_mapped[app->frame_index], &ubo, sizeof(ubo));

    vkCmdBindDescriptorSets(current_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            app->pipeline_layout, 0, 1,
                            &app->descriptor_sets[app->frame_index], 0, NULL);
  }

  // dynamic
  vkCmdSetViewport(current_command_buffer, 0., 1.,
                   &(VkViewport){0., 0., (f32)app->swap_extent.width,
                                 (f32)app->swap_extent.height, 0., 1.});
  vkCmdSetScissor(current_command_buffer, 0., 1.,
                  &(VkRect2D){{0, 0}, app->swap_extent});

  vkCmdDrawIndexed(current_command_buffer, ic, 1, 0, 0, 0);

  vk_end_rendering(app, &current_command_buffer, (u32)image_index);

  return 0;
}

// @main loop
static void main_loop(Application *app) {
  char title[64];
  f64 last_time = now_seconds();
  uint32_t frame_count = 0;
  while (!glfwWindowShouldClose(app->window)) {
    glfwPollEvents();
    draw_frame(app);
    frame_count++;

    f64 current_time = now_seconds();
    f64 elapsed = current_time - last_time;

    if (elapsed >= 1.0) {
      f64 fps = (f64)frame_count / elapsed;

      printf("FPS: %.1f\n", fps);

      frame_count = 0;
      last_time = current_time;
      snprintf(title, sizeof(title), "Vulkan - FPS: %.1f", fps);
      glfwSetWindowTitle(app->window, title);
    }
  }
}

// @main
int main(void) {
  Application app = {
      .vulkan_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE),
      .scratch_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE),
  };

  if (glfw_init(&app) != 0) {
    return EXIT_FAILURE;
  }

  Atlas atlas;
  if (init_atlas(app.vulkan_arena,
                 S("/usr/share/fonts/TTF/CaskaydiaMonoNerdFont-Regular.ttf"),
                 &atlas) != 0) {
    return EXIT_FAILURE;
  };

  // stbi_write_png("font_atlas.png", (int)atlas.w, (int)atlas.h,
  //                1, // 1 canal: R8 / alpha
  //                atlas.data,
  //                (int)atlas.w // stride
  // );

  Str text = S(
      "The quick brown fox jumps over the lazy dog\n"
      "?[{()}]!$<-/#%\\_>`~&:'@^\";|*\n"
      "Porro omnis perspiciatis qui perspiciatis repudiandae. Temporibus iusto "
      "doloribus distinctio. Fuga sint odio nobis culpa aliquam. Non aut aut "
      "illum.\n"
      "Alias occaecati velit aliquid corrupti. Omnis provident sunt laudantium "
      "impedit. Quia dicta illum et.\n"
      "Fuga ullam laudantium consequatur tenetur molestiae. Enim omnis debitis "
      "facere veniam nobis magni. Quo et totam magnam. Tenetur aut ipsum "
      "praesentium. Placeat est omnis laborum vero ducimus et repellendus et.\n"
      "Dicta enim qui doloribus provident ut voluptatum unde. Commodi aut "
      "voluptatibus non consequatur occaecati qui. Dicta minima qui voluptates "
      "cupiditate numquam ad debitis. Culpa ut itaque explicabo deserunt "
      "laboriosam deleniti aut.\n"
      "Quibusdam consectetur nam perferendis aut. Delectus dolor aut assumenda "
      "nemo nisi et. Eum magni impedit blanditiis est et dolores soluta. Ut "
      "harum dolores non suscipit et aut. Fuga facere et quo. Error fuga quo "
      "nostrum.");

  f32 start_x = 20;
  f32 start_y = 256;
  f32 px = start_x;
  f32 py = start_y;
  for (u32 i = 0; i < text.length; i++) {
    u8 c = text.data[i];

    if (c == '\n') {
      printf("car\n");
      py += atlas.line_height;
      px = start_x;
      continue;
    }

    if (c < FIRST_CHAR || c >= FIRST_CHAR + CHAR_COUNT)
      continue;

    u16 base = (u16)vc;
    stbtt_bakedchar *g = &atlas.glyphs[c - FIRST_CHAR];

    f32 x0 = px + g->xoff;
    f32 y0 = py + g->yoff;
    f32 x1 = x0 + (g->x1 - g->x0);
    f32 y1 = y0 + (g->y1 - g->y0);

    f32 u0 = g->x0 / (float)atlas.w;
    f32 v0 = g->y0 / (float)atlas.h;
    f32 u1 = g->x1 / (float)atlas.w;
    f32 v1 = g->y1 / (float)atlas.h;

    vertices[vc++] = BTV(x0, y0, u0, v0);
    vertices[vc++] = BTV(x1, y0, u1, v0);
    vertices[vc++] = BTV(x1, y1, u1, v1);
    vertices[vc++] = BTV(x0, y1, u0, v1);

    indices[ic++] = base + 0;
    indices[ic++] = base + 1;
    indices[ic++] = base + 2;
    indices[ic++] = base + 0;
    indices[ic++] = base + 2;
    indices[ic++] = base + 3;

    px += g->xadvance;
  }

  if (vk_init(&app) != 0) {
    vk_cleanup(&app);
    return EXIT_FAILURE;
  }

  vk_upload_bitmap(&app, atlas.data, atlas.w, atlas.h);

  vk_create_pipeline(&app);
  vk_create_descriptor_set(&app);

  main_loop(&app);
  vk_cleanup(&app);

  return EXIT_SUCCESS;
}

// @font-atlas
int init_atlas(Arena *arena, Str font_path, Atlas *a) {
  *a = (Atlas){.w = 512, .h = 512, .font_size = 18};
  a->data = ARENA_PUSH_ARRAY(arena, a->w * a->h, u8);

  Str ttf_file;
  read_file(arena, font_path, &ttf_file);

  stbtt_fontinfo font_info;
  if (!stbtt_InitFont(&font_info, ttf_file.data, 0)) {
    fprintf(stderr, "Unable to init font.\n");
    return -1;
  };

  int ascent, descent, line_gap;
  stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
  f32 scale = stbtt_ScaleForPixelHeight(&font_info, a->font_size);

  a->ascent = (float)ascent * scale;
  a->descent = (float)descent * scale;
  a->line_height = (float)(ascent - descent + line_gap) * scale;
  printf("%f\n", a->line_height);

  if (!stbtt_BakeFontBitmap(ttf_file.data, 0, a->font_size, a->data, (int)a->w,
                            (int)a->h, FIRST_CHAR, CHAR_COUNT, a->glyphs)) {

    fprintf(stderr, "Unable to bake font atlas bitmap.\n");
    return -1;
  }

  return 0;
}

// int upload_atlas() {}
//
// int cleanup_atlas() {}
