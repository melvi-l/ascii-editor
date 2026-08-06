#define _GNU_SOURCE
#define BASE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "common/common.h"

#ifdef HOT_RELOAD
#include "host/reload.h"
#endif

#include "host/platform.c"
#include "host/vulkan.c"

#ifndef HOT_RELOAD
#include "lib/main.c"
#endif

#ifdef HOT_RELOAD
static Reload g_reload;
#else
static LibAPI g_api;
#endif

// @font-atlas
static int init_atlas(Arena *arena, Str font_path, Atlas *a) {
  *a = (Atlas){.w = 512, .h = 512, .font_size = 16};
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

  stbtt_bakedchar *g = &a->glyphs[' ' - FIRST_CHAR];
  a->advance = (float)g->xadvance;
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

static void host_resize(u32 width, u32 height, void *user_data) {
  Application *app = user_data;
  app->w = width;
  app->h = height;
  rd_resize(app);
#ifdef HOT_RELOAD
  if (g_reload.handle)
    g_reload.api.resize(app, width, height);
#else
  if (g_api.resize)
    g_api.resize(app, width, height);
#endif
}

static void host_key(i32 key, i32 scancode, i32 action, i32 mods,
                     void *user_data) {
  Application *app = user_data;
#ifdef HOT_RELOAD
  if (g_reload.handle)
    g_reload.api.key(app, key, scancode, action, mods);
#else
  if (g_api.key)
    g_api.key(app, key, scancode, action, mods);
#endif
}

static void host_char(u32 c, void *user_data) {
  Application *app = user_data;
#ifdef HOT_RELOAD
  if (g_reload.handle)
    g_reload.api.char_input(app, c);
#else
  if (g_api.char_input)
    g_api.char_input(app, c);
#endif
}

int main(int argc, char **argv) {
  Str filepath;
  if (argc < 2) {
    filepath = S("lorem.txt");
  } else {
    filepath = S_line(argv[1]);
  }

  printf("%*s\n", STR_FMT(filepath));

  Application app = {
      .vulkan_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE),
      .scratch_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE),
  };

  if (plat_init(&app.plat, host_resize, &app) != 0) {
    return EXIT_FAILURE;
  }

  plat_set_char_callback(&app.plat, host_char);
  plat_set_key_callback(&app.plat, host_key);

  if (init_atlas(
          app.vulkan_arena,
          S("/usr/share/fonts/adwaita-mono-fonts/AdwaitaMono-Regular.ttf"),
          &app.atlas) != 0) {
    return EXIT_FAILURE;
  }

  Editor editor = {.color = {.R = 1., .G = 1., .B = 1., .A = 1.},
                   .wrap_enabled = true};

  Arena *layout_arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE);
  editor.layout = (Layout){
      .arena = layout_arena,
      .line_height = app.atlas.line_height,
      .glyph_advance = app.atlas.advance,
  };

  app.editor = editor;

  if (!read_file(app.vulkan_arena, filepath, &app.editor.doc.table)) {
    fprintf(stderr, "Unable to open editor file\n");
    goto cleanup;
  }

  app.quad_list = (QuadInstanceList){
      .data = ARENA_PUSH_ARRAY(app.vulkan_arena, max_quad_count, QuadInstance),
      .length = 0,
      .capacity = max_quad_count,
  };

  plat_get_window_size(&app.plat, &app.w, &app.h);

  if (!rd_create_instance(&app)) {
    goto cleanup;
  }

  if (!plat_create_vulkan_surface(app.plat.window, app.instance, NULL,
                                  &app.surface)) {
    goto cleanup;
  }

  if (!rd_init(&app)) {
    goto cleanup;
  }

  rd_upload_bitmap(&app, app.atlas.data, app.atlas.w, app.atlas.h);
  rd_create_pipeline(&app);
  rd_create_descriptor_set(&app);

#ifdef HOT_RELOAD
  g_reload = (Reload){
      .path = "./build/libapp.so",
      .app = &app,
  };
  if (!reload_open(&g_reload)) {
    fprintf(stderr, "host: failed to open lib\n");
    goto cleanup;
  }
#else
  g_api = lib_get_api();
  g_api.load(&app);
#endif

  f64 _fps = 0;
  Timer fps_timer = {.interval_ms = 500};
  Timer reasonnable_timer = {.interval_ms = 5000};
  while (!plat_should_close(&app.plat)) {
    plat_poll_events();

    _fps = plat_compute_fps();
    if (timer_tick(&fps_timer)) {
      app.fps = _fps;
    }

    if (timer_tick(&reasonnable_timer)) {
#ifdef HOT_RELOAD
      reload_poll(&g_reload);
#endif
    }
#ifdef HOT_RELOAD
    g_reload.api.update(&app);
#else
    g_api.update(&app);
#endif
  }

#ifdef HOT_RELOAD
  reload_close(&g_reload);
#else
  g_api.unload(&app);
#endif

cleanup:
  rd_cleanup(&app);
  plat_destroy(&app.plat);
  return EXIT_SUCCESS;
}
