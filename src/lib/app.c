#include "common/common.h"
#include "host/vulkan.h"
#include <vulkan/vulkan_core.h>

#define FONT_SIZE 16
#define MARGIN 16
#define MARGIN_X MARGIN
#define MARGIN_Y MARGIN
#define PADDING 32
#define PADDING_X PADDING
#define PADDING_Y PADDING

// mirror of rd.c quad indices (same TU-independent macro)
#define indices_count 6

static u32 render_command_execute(Application *app, RenderCommand cmd,
                                  QuadInstanceList *list) {
  switch (cmd.kind) {
  case RENDER_COMMAND_RECT:
    if (list->capacity - list->length <= 0) {
      fprintf(stderr, "No more capacity in quad instance list\n");
      return 0;
    }
    list->data[list->length++] = (QuadInstance){
        .x = cmd.bounding_box.x,
        .y = cmd.bounding_box.y,
        .w = cmd.bounding_box.w,
        .h = cmd.bounding_box.h,
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
    f32 px = cmd.bounding_box.x;
    f32 py = cmd.bounding_box.y + (f32)app->atlas.ascent;
    for (u32 i = 0; i < cmd.text.str.length; i++) {
      u8 c = cmd.text.str.data[i];

      if (c == '\n') {
        px = cmd.bounding_box.x;
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

      if (px < cmd.bounding_box.x ||
          px > cmd.bounding_box.x + cmd.bounding_box.w ||
          py < cmd.bounding_box.y ||
          py > cmd.bounding_box.y + cmd.bounding_box.h) {
        px = cmd.bounding_box.x;
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
int compute_text_size(Application *app, Str str, f32 *width, f32 *height,
                      Rect bounding_box) {

  if (str.data[str.length] == '\n') {
    str.length--;
  }

  Atlas atlas = app->atlas;
  f32 px = 0;
  f32 py = atlas.ascent;
  u8 *c;
  *width = 0, *height = 0;
  for (u32 i = 0; i < str.length - 1; i++) {
    c = &str.data[i];

    if (px < 0 || px > bounding_box.w || py < 0 || py > bounding_box.h) {
      *width = max(*width, px);
      py += atlas.line_height;
      px = 0;
      continue;
    }

    if (*c == '\n') {
      *width = max(*width, px);
      py += atlas.line_height;
      px = 0;
      continue;
    }

    if (*c < FIRST_CHAR || *c >= FIRST_CHAR + CHAR_COUNT)
      continue;

    stbtt_bakedchar *g = &atlas.glyphs[*c - FIRST_CHAR];
    px += g->xadvance;
  }

  *height = py;
  *width = max(*width, px);
  return 0;
}

// TODO leverage tree like datastructrue
static i32 find_line_start(Str text, i32 pos) {
  while (pos > 0 && text.data[pos - 1] != '\n') {
    pos--;
  }

  return pos;
}

static i32 find_line_end(Str text, i32 start) {
  i32 pos = start;

  while (pos < (i32)text.length && text.data[pos] != '\n') {
    pos++;
  }

  return pos;
}

int compute_cursor(Cursor *cursor, Str text, i32 d_row, i32 d_col) {
  i32 length = (i32)text.length;
  i32 old_pos = (i32)cursor->text_pos;

  if (old_pos < 0) {
    old_pos = 0;
  } else if (old_pos > length) {
    old_pos = length;
  }

  i32 current_line_start = find_line_start(text, old_pos);
  i32 target_line_start = current_line_start;
  i32 row_advance = 0;

  while (row_advance < d_row) {
    i32 line_end = find_line_end(text, target_line_start);
    printf("increase line (%i on %i): end of current line %i => %i\n",
           row_advance, d_row, (i32)cursor->_row + row_advance + 1, line_end);

    if (line_end >= length) {
      break;
    }

    target_line_start = line_end + 1;
    printf("increase line (%i on %i): start of line %i => %i\n", row_advance,
           d_row, (i32)cursor->_row + row_advance + 1, target_line_start);
    row_advance++;
  }

  while (row_advance > d_row) {
    if (target_line_start == 0) {
      break;
    }

    target_line_start = find_line_start(text, target_line_start - 1);
    printf("decrease line (%i on %i): start of line %i => %i\n", row_advance,
           d_row, row_advance + 1, target_line_start);

    row_advance--;
  }

  i32 current_col = old_pos - current_line_start;
  i32 target_col;

  if (row_advance != 0) {
    target_col = (i32)cursor->desired_col + d_col;
  } else {
    target_col = current_col + d_col;
  }

  if (target_col < 0) {
    target_col = 0;
  }

  i32 target_line_end = find_line_end(text, target_line_start);
  i32 target_line_length = target_line_end - target_line_start;

  if (target_col > target_line_length) {
    target_col = target_line_length;
  }

  i32 new_pos = target_line_start + target_col;
  i32 new_row = (i32)cursor->_row + row_advance;

  if (new_row < 0) {
    new_row = 0;
  }

  cursor->text_pos = (u32)new_pos;
  cursor->_row = (u32)new_row;
  cursor->_col = (u32)target_col;

  return new_pos - old_pos;
}

//
int compute_cursor_rect(Application *app, Cursor cursor, Str text, Rect *rect) {
  // compute per line x offset
  (void)&text;
  *rect = (Rect){
      .x = app->editor_viewport.x + app->atlas.advance * (f32)cursor._col,
      .y = app->editor_viewport.y + app->atlas.line_height * (f32)cursor._row,
      .w = 2,
      .h = app->atlas.line_height};

  return 0;
}

int compute_frame(Application *app) {
  app->quad_list.length = 0;

  Rect outer = (Rect){
      .x = MARGIN_X,
      .y = MARGIN_Y,
      .w = (f32)app->w - MARGIN_X * 2,
      .h = (f32)app->h - MARGIN_Y * 2 - 64,
  };

  app->editor_viewport = (Rect){
      .x = outer.x + PADDING_X,
      .y = outer.y + PADDING_Y,
      .w = outer.w - PADDING_X * 2,
      .h = outer.h - PADDING_Y * 2,
  };

  Rect information_viewport = {
      .x = outer.x,
      .y = outer.y + outer.h - 64,
      .w = outer.w,
      .h = 64,
  };
  // rect
  {
    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .bounding_box =
                {
                    .x = outer.x,
                    .y = outer.y,
                    .w = outer.w,
                    .h = outer.h,
                },
            .rect = {.color = {.R = .01f, .G = .01f, .B = .01f, .A = 1}}},
        &app->quad_list);
  }

  // text
  {
    if (render_command_execute(
            app,
            (RenderCommand){
                .kind = RENDER_COMMAND_TEXT,
                .bounding_box = app->editor_viewport,
                .text = {.str = app->editor_text,
                         .color = {.R = 1., .G = 1., .B = 1., .A = .8f}}},
            &app->quad_list) != app->editor_text.length) {
      fprintf(stderr, "Unable to draw editor text.\n");
    };

    // green
    f32 w, h;
    compute_text_size(app, app->editor_text, &w, &h, app->editor_viewport);
    render_command_execute(
        app,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .bounding_box =
                {
                    .x = app->editor_viewport.x,
                    .y = app->editor_viewport.y + h,
                    .w = w,
                    .h = 4,
                },
            .rect = {.color = {.R = 0., .G = 1., .B = .0, .A = 0.}}},
        &app->quad_list);
  }

  // cursor
  Rect cursor_rect;
  compute_cursor_rect(app, app->editor_cursor, app->editor_text, &cursor_rect);
  render_command_execute(
      app,
      (RenderCommand){.kind = RENDER_COMMAND_RECT,
                      .bounding_box = cursor_rect,
                      .rect = {.color = {.R = 1., .G = 1., .B = 1., .A = 1.}}},
      &app->quad_list);

  // information
  ArenaTemp temp = arena_temp_begin(app->scratch_arena);
  Str information_str = str_format(
      temp.arena, "col=%u ; row=%u ;\ntext_advance=%u", app->editor_cursor._col,
      app->editor_cursor._row, app->editor_cursor.text_pos);
  render_command_execute(
      app,
      (RenderCommand){.kind = RENDER_COMMAND_RECT,
                      .bounding_box = information_viewport,
                      .rect = {.color = {.R = 1., .G = 0., .B = 0., .A = 1.}}},
      &app->quad_list);
  render_command_execute(
      app,
      (RenderCommand){.kind = RENDER_COMMAND_TEXT,
                      .bounding_box = information_viewport,
                      .text = {.str = information_str,
                               .color = {.R = 1., .G = 1., .B = 1., .A = 1.}}},
      &app->quad_list);

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

// @lib-callbacks
void on_resize(Application *app, u32 w, u32 h) {
  app->w = w;
  app->h = h;
  compute_frame(app);
}

void on_key(Application *app, i32 key, i32 scancode, i32 action, i32 mods) {
  (void)scancode;
  (void)mods;
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    if (key == GLFW_KEY_UP) {
      compute_cursor(&app->editor_cursor, app->editor_text, -1, 0);
    }
    if (key == GLFW_KEY_DOWN) {
      compute_cursor(&app->editor_cursor, app->editor_text, 1, 0);
    }

    if (key == GLFW_KEY_LEFT) {
      compute_cursor(&app->editor_cursor, app->editor_text, 0, -1);
    }
    if (key == GLFW_KEY_RIGHT) {
      compute_cursor(&app->editor_cursor, app->editor_text, 0, 1);
    }
  }
}

void on_char_input(Application *app, u32 c) {
  (void)app;
  printf("on_char %c\n", c);
}
