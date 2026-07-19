#include "main.h"

#include "plat.c"
#include "rd.c"

#define TV(x, y, r, g, b, u, v)                                                \
  ((TextVertex){.position = {.X = x, .Y = y},                                  \
                .color = {.R = r, .G = g, .B = b},                             \
                .uv = {.U = u, .V = v}})

#define BTV(x, y, u, v) TV(x, y, 1, 1, 1, u, v)

int init_atlas(Arena *arena, Str font_path, Atlas *a);

int compute_frame(Application *app) {
  Atlas *atlas = &app->atlas;
  app->editor_viewport = (Viewport){.x = 64,
                                    .y = 64,
                                    .w = app->w - 64 * 2,
                                    .h = app->h - 64 * 2,
                                    .padding_h = 32,
                                    .padding_v = 32};
  // text
  u32 ic = 0;

  f32 start_x =
      (f32)app->editor_viewport.x + (f32)app->editor_viewport.padding_h;
  f32 start_y =
      (f32)app->editor_viewport.y + (f32)app->editor_viewport.padding_v;
  f32 end_x = (f32)app->editor_viewport.x + (f32)app->editor_viewport.w -
              (f32)app->editor_viewport.padding_h;
  f32 end_y = (f32)app->editor_viewport.y + (f32)app->editor_viewport.h -
              (f32)app->editor_viewport.padding_v;

  printf("Compute frame with %f, %f, %f, %f\n", start_x, start_y, end_x, end_y);
  f32 px = start_x;
  f32 py = start_y;
  for (u32 i = 0; i < app->editor_text.length; i++) {
    u8 c = app->editor_text.data[i];

    if (c == '\n') {
      px = start_x;
      py += atlas->line_height;
      continue;
    }

    if (c < FIRST_CHAR || c >= FIRST_CHAR + CHAR_COUNT)
      continue;

    stbtt_bakedchar *g = &atlas->glyphs[c - FIRST_CHAR];

    if (px < start_x || px > end_x || py < start_y || py > end_y) {
      // printf("%c -- out of bound\n", c);
      // px += g->xadvance;
      px = start_x;
      py += atlas->line_height;
      continue;
    }

    instances[ic++] = (GlyphInstance){.x = px + g->xoff,
                                      .y = py + g->yoff,
                                      .w = g->x1 - g->x0,
                                      .h = g->y1 - g->y0,
                                      .u_min_x = g->x0 / (float)atlas->w,
                                      .u_min_y = g->y0 / (float)atlas->h,
                                      .u_max_x = g->x1 / (float)atlas->w,
                                      .u_max_y = g->y1 / (float)atlas->h,
                                      .r = 1.,
                                      .g = 1.,
                                      .b = 1.};

    px += g->xadvance;
  }
  app->editor_glyph_count = ic;
  app->editor_glyph_is_dirty = true;

  return 0;
}

int draw_frame(Application *app) {
  VkCommandBuffer current_command_buffer = NULL;
  i32 image_index = rd_begin_rendering(app, &current_command_buffer);
  if (image_index < 0)
    perror("failed to begin rendering");

  // text
  {
    vkCmdBindPipeline(current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      app->pipeline);
    VkBuffer vertex_buffers[] = {app->geometry_buffer, app->instance_buffer};
    VkDeviceSize offsets[] = {app->vertex_offset, 0};
    vkCmdBindVertexBuffers(current_command_buffer, 0, 2, vertex_buffers,
                           offsets);
    vkCmdBindIndexBuffer(current_command_buffer, app->geometry_buffer,
                         app->index_offset, VK_INDEX_TYPE_UINT16);

    // instance buffer
    if (app->editor_glyph_is_dirty) {
      memcpy(app->instance_mapped_array, instances,
             sizeof(GlyphInstance) * app->editor_glyph_count);
    }

    // uniform
    UniformBufferObject ubo = {};
    ubo.proj = Orthographic_RH_ZO(0.f, (f32)app->swap_extent.width, 0.f,
                                  (f32)app->swap_extent.height, -1.f, 1.f);

    memcpy(app->uniform_mapped_arrays[app->frame_index], &ubo, sizeof(ubo));
    vkCmdBindDescriptorSets(current_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            app->pipeline_layout, 0, 1,
                            &app->descriptor_sets[app->frame_index], 0, NULL);

    // dynamic
    vkCmdSetViewport(current_command_buffer, 0., 1.,
                     &(VkViewport){0., 0., (f32)app->swap_extent.width,
                                   (f32)app->swap_extent.height, 0., 1.});
    vkCmdSetScissor(current_command_buffer, 0., 1.,
                    &(VkRect2D){{0, 0}, app->swap_extent});

    vkCmdDrawIndexed(current_command_buffer, indices_count,
                     app->editor_glyph_count, 0, 0, 0);
  }

  rd_end_rendering(app, &current_command_buffer, (u32)image_index);

  return 0;
}

// @main loop
void main_loop(Application *app, Atlas *atlas) {
  (void)atlas;
  Timer second_timer = {.interval = 1.};
  while (!plat_should_close(&app->plat)) {
    plat_poll_events();
    draw_frame(app);
    f64 fps = plat_compute_fps();
    if (timer_tick(&second_timer)) {
      printf("FPS: %.1f\n", fps);
    }
  }
}

void resize(u32 width, u32 height, void *user_data) {
  Application *app = user_data;
  if (app == NULL)
    return;

  f64 start_time = now_seconds();

  app->w = width;
  app->h = height;
  rd_resize(app);
  compute_frame(app);

  f64 end_time = now_seconds();
  f64 elapsed = end_time - start_time;
  printf("resizing take %.2fms.\n", elapsed * 1000);
}

// @main
int main(void) {
  Application app = {
      .vulkan_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE),
      .scratch_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE),
  };

  if (plat_init(&app.plat, resize, &app) != 0) {
    return EXIT_FAILURE;
  }

  if (init_atlas(app.vulkan_arena,
                 S("/usr/share/fonts/TTF/CaskaydiaMonoNerdFont-Regular.ttf"),
                 &app.atlas) != 0) {
    return EXIT_FAILURE;
  };

  plat_get_window_size(&app.plat, &app.w, &app.h);

  app.editor_cursor = (Cursor){.col = 0, .row = 0};

  app.editor_text =
      S("The quick brown fox jumps over the lazy dog\n"
        "?[{()}]!$<-/#%\\_>`~&:'@^\";|*\n"
        "Porro omnis perspiciatis qui perspiciatis repudiandae. Temporibus "
        "iusto "
        "doloribus distinctio. Fuga sint odio nobis culpa aliquam. Non aut aut "
        "illum.\n"
        "Alias occaecati velit aliquid corrupti. Omnis provident sunt "
        "laudantium "
        "impedit. Quia dicta illum et.\n"
        "Fuga ullam laudantium consequatur tenetur molestiae. Enim omnis "
        "debitis "
        "facere veniam nobis magni. Quo et totam magnam. Tenetur aut ipsum "
        "praesentium. Placeat est omnis laborum vero ducimus et repellendus "
        "et.\n"
        "Dicta enim qui doloribus provident ut voluptatum unde. Commodi aut "
        "voluptatibus non consequatur occaecati qui. Dicta minima qui "
        "voluptates "
        "cupiditate numquam ad debitis. Culpa ut itaque explicabo deserunt "
        "laboriosam deleniti aut.\n"
        "Quibusdam consectetur nam perferendis aut. Delectus dolor aut "
        "assumenda "
        "nemo nisi et. Eum magni impedit blanditiis est et dolores soluta. Ut "
        "harum dolores non suscipit et aut. Fuga facere et quo. Error fuga quo "
        "nostrum.");

  // move in loop, update dirty host visible buffer
  compute_frame(&app);

  if (!rd_create_instance(&app)) {
    goto cleanup;
  }

  if (!plat_create_vulkan_surface(app.plat.window, app.instance, NULL,
                                  &app.surface)) {
    goto cleanup;
  };

  if (!rd_init(&app)) {
    goto cleanup;
  }

  rd_upload_bitmap(&app, app.atlas.data, app.atlas.w, app.atlas.h);

  rd_create_pipeline(&app);
  rd_create_descriptor_set(&app);

  main_loop(&app, &app.atlas);
  rd_cleanup(&app);

  return EXIT_SUCCESS;

cleanup:
  rd_cleanup(&app);
  return EXIT_FAILURE;
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
