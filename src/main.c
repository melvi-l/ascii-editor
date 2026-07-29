#include "main.h"

#include "base.h"
#include "plat.c"
#include "rd.c"

#define FONT_SIZE 16
#define MARGIN 16
#define MARGIN_X MARGIN
#define MARGIN_Y MARGIN
#define PADDING 32
#define PADDING_X PADDING
#define PADDING_Y PADDING

#define max_quad_count (1 << 14)

int init_atlas(Arena *arena, Str font_path, Atlas *a);

static u32 render_command_execute(Application *app, RenderCommand cmd,
                                  QuadInstanceList *list) {
  switch (cmd.kind) {
  case RENDER_COMMAND_RECT:
    if (list->capacity - list->length <= 0) {
      fprintf(stderr, "No more capacity in quad instance list\n");
      return 0;
    }
    list->data[list->length++] = (QuadInstance){
        .x = cmd.boundingBox.x,
        .y = cmd.boundingBox.y,
        .w = cmd.boundingBox.w,
        .h = cmd.boundingBox.h,
        .u_min_x = app->atlas.white_x,
        .u_min_y = app->atlas.white_y,
        .u_max_x = app->atlas.white_x,
        .u_max_y = app->atlas.white_y,
        .r = cmd.rect.color.R,
        .g = cmd.rect.color.G,
        .b = cmd.rect.color.B,
    };
    return 1;
  case RENDER_COMMAND_TEXT:
    Atlas *atlas = &app->atlas;
    f32 px = cmd.boundingBox.x;
    f32 py = cmd.boundingBox.y + (f32)app->atlas.ascent;
    for (u32 i = 0; i < cmd.text.str.length; i++) {
      u8 c = cmd.text.str.data[i];

      if (c == '\n') {
        px = cmd.boundingBox.x;
        py += atlas->line_height;
        continue;
      }

      if (c < FIRST_CHAR || c >= FIRST_CHAR + CHAR_COUNT)
        continue;

      stbtt_bakedchar *g = &atlas->glyphs[c - FIRST_CHAR];

      if (c == ' ') {
        px += g->xadvance;
        continue;
      }

      if (px < cmd.boundingBox.x ||
          px > cmd.boundingBox.x + cmd.boundingBox.w //||
          // py < cmd.boundingBox.y ||
          // py > cmd.boundingBox.y + cmd.boundingBox.h
      ) {
        px = cmd.boundingBox.x;
        py += atlas->line_height;
        continue;
      }

      f32 glyph_x = roundf(px + g->xoff);
      f32 glyph_y = roundf(py + g->yoff);

      if (list->capacity - list->length <= 0) {
        return i;
      }

      list->data[list->length++] = (QuadInstance){
          .x = glyph_x,
          .y = glyph_y,
          .w = (f32)(g->x1 - g->x0),
          .h = (f32)(g->y1 - g->y0),
          .u_min_x = (f32)g->x0 / (f32)atlas->w,
          .u_min_y = (f32)g->y0 / (f32)atlas->h,
          .u_max_x = (f32)g->x1 / (f32)atlas->w,
          .u_max_y = (f32)g->y1 / (f32)atlas->h,
          .r = cmd.text.color.R,
          .g = cmd.text.color.G,
          .b = cmd.text.color.B,
          // .a = cmd.rect.color.A,
      };

      px += g->xadvance;
    }

    return (u32)cmd.text.str.length;

  case RENDER_COMMAND_IMAGE:
    return 0;

  default:
    fprintf(stderr, "Unknown render command kind.\n");
    return 0;
  }
}

// TODO: add wrapping flag
int compute_text_size(Application *app, Str *str, f32 *width, f32 *height,
                      f32 max_width) {

  Atlas atlas = app->atlas;
  f32 px = 0;
  *width = 0, *height = atlas.line_height;
  for (u32 i = 0; i < str->length; i++) {
    u8 *c = &str->data[i];

    if (*c == '\n' || (max_width != 0 && px > max_width)) {
      *height += atlas.line_height;
      *width = max(*width, px);
      px = 0;
      continue;
    }

    if (*c < FIRST_CHAR || *c >= FIRST_CHAR + CHAR_COUNT)
      continue;

    stbtt_bakedchar *g = &atlas.glyphs[*c - FIRST_CHAR];
    px += g->xadvance;
  }

  // *height += atlas.line_height;
  *width = max(*width, px);
  return 0;
}

int compute_frame(Application *app) {
  app->quad_list.length = 0;
  // printf("window: %u x %u, framebuffer: %u x %u\n", app->w, app->h,
  //        app->swap_extent.width, app->swap_extent.height);

  Rect outer = (Rect){
      .x = MARGIN_X,
      .y = MARGIN_Y,
      .w = (f32)app->w - MARGIN_X * 2,
      .h = (f32)app->h - MARGIN_Y * 2,
  };

  app->editor_viewport = (Rect){
      .x = outer.x + PADDING_X,
      .y = outer.y + PADDING_Y,
      .w = outer.w - PADDING_X * 2,
      .h = outer.h - PADDING_Y * 2,
  };
  // rect
  {
    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .boundingBox =
                {
                    .x = outer.x,
                    .y = outer.y,
                    .w = outer.w,
                    .h = outer.h,
                },
            .rect = {.color = {.R = .05f, .G = .05f, .B = .05f, .A = 1}}},
        &app->quad_list);
  }

  // text
  f32 w, h;
  f32 start_x = (f32)app->editor_viewport.x;
  f32 start_y = (f32)app->editor_viewport.y;
  {

    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .boundingBox = {.x = start_x,
                            .y = start_y,
                            .w = (float)app->editor_viewport.w,
                            .h = (float)app->atlas.line_height},
            .rect = {.color = {.R = 1., .G = 0., .B = 0., .A = 0.}}},
        &app->quad_list);

    if (render_command_execute(
            app,
            (RenderCommand){
                .kind = RENDER_COMMAND_TEXT,
                .boundingBox =
                    {
                        .x = start_x,
                        .y = start_y,
                        .w = (float)app->editor_viewport.w,
                        .h = (float)app->editor_viewport.h,
                    },
                .text = {.str = app->editor_text,
                         .color = {.R = 0., .G = 1., .B = 1., .A = 1.}}},
            &app->quad_list) != app->editor_text.length) {
      fprintf(stderr, "Unable to draw editor text.\n");
    };
    compute_text_size(app, &app->editor_text, &w, &h,
                      (float)app->editor_viewport.w);
    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .boundingBox =
                {
                    .x = start_x,
                    .y = start_y + h,
                    .w = w,
                    .h = 4,
                },
            .rect = {.color = {.R = 0., .G = 1., .B = .0, .A = 0.}}},
        &app->quad_list);
  }

  // button
  {

    Str button_text =
        S("click me\nplease\nidk what i am about to seses eses\ne\nesesesesse");
    f32 width, height;
    if (compute_text_size(app, &button_text, &width, &height, 0) != 0) {
      fprintf(stderr, "Unable to compute text size.\n");
      return 1;
    };
    // printf("computed size: %f %f\n", width, height);
    Rect button = {
        .x = 256, .y = h + 256, .w = width + 2 * 32, .h = height + 2 * 16};

    Rect inner = {
        .x = button.x + 32, .y = button.y + 16, .w = width, .h = height};

    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .boundingBox =
                {
                    .x = (float)button.x,
                    .y = (float)button.y,
                    .w = (float)button.w,
                    .h = (float)button.h,
                },
            .rect = {.color = {.R = 0., .G = 0., .B = .5, .A = 0.}}},
        &app->quad_list);

    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .boundingBox =
                {
                    .x = inner.x,
                    .y = inner.y,
                    .w = inner.w,
                    .h = inner.h,
                },
            .rect = {.color = {.R = .5, .G = 0., .B = 0., .A = 0.}}},
        &app->quad_list);

    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_TEXT,
            .boundingBox =
                {
                    .x = inner.x,
                    .y = inner.y,
                    .w = inner.w,
                    .h = inner.h,
                },
            .text = {.str = button_text,
                     .color = {.R = 1., .G = 1., .B = 1., .A = 1.}}},
        &app->quad_list);
  }

  app->editor_quad_is_dirty = true;
  return 0;
}

int draw_frame(Application *app) {
  VkCommandBuffer current_command_buffer = NULL;
  i32 image_index = rd_begin_rendering(app, &current_command_buffer);
  if (image_index < 0)
    perror("failed to begin rendering");

  // text
  {
    rd_bind_pipeline(app, current_command_buffer);

    // instance buffer
    if (app->editor_quad_is_dirty) {
      memcpy(app->instance_mapped_array, app->quad_list.data,
             sizeof(QuadInstance) * app->quad_list.length);
    }

    // uniform
    UniformBufferObject ubo = {};
    ubo.proj =
        Orthographic_RH_ZO(0.f, (f32)app->w, 0.f, (f32)app->h, -1.f, 1.f);
    rd_upload_uniforms(app, current_command_buffer, ubo);

    // dynamic
    rd_set_dynamic(app, current_command_buffer);

    vkCmdDrawIndexed(current_command_buffer, indices_count,
                     app->quad_list.length, 0, 0, 0);
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
    compute_frame(app);
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

  app.quad_list = (QuadInstanceList){
      .data = ARENA_PUSH_ARRAY(app.vulkan_arena, max_quad_count, QuadInstance),
      .length = 0,
      .capacity = max_quad_count,
  };

  if (plat_init(&app.plat, resize, &app) != 0) {
    return EXIT_FAILURE;
  }

  if (init_atlas(
          app.vulkan_arena,
          S("/usr/share/fonts/adwaita-mono-fonts/AdwaitaMono-Regular.ttf"),
          &app.atlas) != 0) {
    return EXIT_FAILURE;
  };

  plat_get_window_size(&app.plat, &app.w, &app.h);

  app.editor_cursor = (Cursor){.col = 0, .row = 0};

  app.editor_text =
      S("The quick brown fox jumps over the lazy dog\n"
        "?[{()}]!$<-/#%\\_>`~&:'@^\";|*\n"
        "123456789\n"
        "123456789\n"
        "Porro omnis perspiciatis qui perspiciatis repudiandae. Temporibus "
        "iusto "
        "doloribus distinctio. Fuga sint odio nobis culpa aliquam. Non aut aut "
        "illum.\n"
        "123456789\n"
        "Alias occaecati velit aliquid corrupti. Omnis provident sunt "
        "laudantium "
        "impedit. Quia dicta illum et.\n"
        "Fuga ullam laudantium consequatur tenetur molestiae. Enim omnis "
        "123456789\n"
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
  *a = (Atlas){.w = 512, .h = 512, .font_size = FONT_SIZE};
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

  if (!stbtt_BakeFontBitmap(ttf_file.data, 0, a->font_size, a->data, (int)a->w,
                            (int)a->h, FIRST_CHAR, CHAR_COUNT, a->glyphs)) {

    fprintf(stderr, "Unable to bake font atlas bitmap.\n");
    return -1;
  }

  // white texture area to draw rect
  u32 max_glyph_y = 0;

  for (u32 i = 0; i < CHAR_COUNT; ++i) {
    if (a->glyphs[i].y1 > max_glyph_y) {
      max_glyph_y = a->glyphs[i].y1;
    }
  }

  const u32 white_region_size = 4;
  const u32 white_region_padding = 2;

  u32 white_x = white_region_padding;
  u32 white_y = max_glyph_y + white_region_padding;

  if (white_x + white_region_size > (u32)a->w ||
      white_y + white_region_size > (u32)a->h) {
    fprintf(stderr, "No room left for the white atlas region.\n");
    return -1;
  }

  for (u32 y = 0; y < white_region_size; ++y) {
    for (u32 x = 0; x < white_region_size; ++x) {
      u32 atlas_x = white_x + x;
      u32 atlas_y = white_y + y;

      a->data[atlas_y * a->w + atlas_x] = 255;
    }
  }

  a->white_x = ((f32)white_x + (f32)white_region_size * 0.5f) / (f32)a->w;
  a->white_y = ((f32)white_y + (f32)white_region_size * 0.5f) / (f32)a->h;

  return 0;
}
