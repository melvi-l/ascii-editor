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

#define indices_count 6

static f64 mouse_x = 0, mouse_y = 0;
#define editor_mouse_x(__x) (__x) - app->editor.vp.x - app->editor.vp.scroll_x
#define editor_mouse_y(__y) (__y) - app->editor.vp.y + app->editor.vp.scroll_y

typedef enum InfoMode {
  INFO_ROW,
  INFO_SCROLL,
  INFO_GRAPHICS,
} InfoMode;
static InfoMode info_mode = INFO_ROW;

// @atlas
static const stbtt_bakedchar *atlas_get_glyph(const Atlas *a, const char c) {
  return &a->glyphs[c - FIRST_CHAR];
}

// @render-command
u32 render_command_execute(const Atlas *atlas, RenderCommand cmd,
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
        .u_min_x = atlas->white_x,
        .u_min_y = atlas->white_y,
        .u_max_x = atlas->white_x,
        .u_max_y = atlas->white_y,
        .r = cmd.rect.color.R,
        .g = cmd.rect.color.G,
        .b = cmd.rect.color.B,
        .a = cmd.rect.color.A,
    };
    return 1;
  case RENDER_COMMAND_TEXT:
    f32 px = cmd.bounding_box.x;
    f32 baseline_y = cmd.bounding_box.y + (f32)atlas->ascent;
    for (u32 i = 0; i < cmd.text.str.length; i++) {
      char c = (char)cmd.text.str.data[i];

      if (c == '\n') {
        px = cmd.bounding_box.x;
        baseline_y += atlas->line_height;
        continue;
      }

      if (c < FIRST_CHAR)
        continue;

      const stbtt_bakedchar *g = atlas_get_glyph(atlas, c);
      f32 glyph_x = roundf(px + g->xoff);
      f32 glyph_y = roundf(baseline_y + g->yoff);

      if (c == ' ') {
        px += g->xadvance;
        continue;
      }

      if (glyph_x + g->xadvance < cmd.bounding_box.x ||
          glyph_x > cmd.bounding_box.x + cmd.bounding_box.w ||
          glyph_y + atlas->line_height < cmd.bounding_box.y ||
          glyph_y > cmd.bounding_box.y + cmd.bounding_box.h) {
        // px = cmd.bounding_box.x;
        // py += atlas->line_height;
        continue;
      }

      assert(list->length <= list->capacity);
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
          .a = cmd.text.color.A,
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

//
// @text
// static i32 find_previous_char(Str text, i32 pos, char c) {
//   while (pos >= 0 && text.data[pos] != c) {
//     pos--;
//   }
//
//   return pos;
// }
// static i32 find_next_char(Str text, i32 pos, char c) {
//   while (pos < (i32)text.length && text.data[pos] != c) {
//     pos++;
//   }
//
//   return pos;
// }
#define find_line_start(str, pos) (find_previous_char((str), (pos), '\n') - 1)
#define find_line_end(str, pos) (find_next_char((str), (pos), '\n'))

// TODO(melvil): need upgrade
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

//
// @document
typedef enum CharacterClass {
  CHARACTER_SPACE,
  CHARACTER_LINE_END,
  CHARACTER_WORD,
  CHARACTER_PUNCTUATION
} CharacterClass;
CharacterClass character_class(char byte) {
  if (byte == ' ') {
    return CHARACTER_SPACE;
  }

  if (byte == '\n') {
    return CHARACTER_LINE_END;
  }

  if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
      (byte >= '0' && byte <= '9') || byte == '_') {
    return CHARACTER_WORD;
  }

  return CHARACTER_PUNCTUATION;
}
static u32 doc_get_length(const Document doc) { return (u32)doc.table.length; }
static char doc_get_char(const Document doc, u32 offset) {
  assert(offset < doc_get_length(doc));
  return (char)doc.table.data[offset];
}
static u32 doc_previous_offset(const Document doc, u32 offset) {
  (void)doc;
  if (offset > 0)
    return offset - 1;
  return 0;
}
static u32 doc_next_offset(const Document doc, u32 offset) {
  u32 len = doc_get_length(doc);
  if (offset < len)
    return offset + 1;
  return len;
}

//
// @layout
// static bool layout_is_valid(const Layout layout, const Document doc) {
//   return layout.document_revision == doc.revision;
// }

#define LAYOUT_ROW_TOLERANCE 10
static u32 layout_estimate_row_count(const Layout layout, const Document doc,
                                     bool wrap_enabled) {
  if (!wrap_enabled) {
    // TODO(melvil): optimize
    u32 hard_break_count = 0;
    u32 len = doc_get_length(doc);
    for (u32 i = 0; i < len; i++) {
      if (doc_get_char(doc, i) == '\n') {
        hard_break_count++;
      }
    }
    return hard_break_count + 1;
  }
  return (u32)((float)doc_get_length(doc) / layout.glyph_advance *
               LAYOUT_ROW_TOLERANCE);
}

#define TAB_WIDTH 4
static float layout_get_advance(const Layout layout, const char c) {
  if (c == '\t')
    return TAB_WIDTH * layout.glyph_advance;
  return layout.glyph_advance;
}

static LayoutRow *layout_find_row(const Layout *layout, u32 offset,
                                  u32 *out_index) {
  assert(layout != NULL || layout->rows != NULL);

  for (u32 i = 0; i < layout->row_count; ++i) {
    LayoutRow *row = &layout->rows[i];

    if (offset >= row->offset_start && offset <= row->offset_end) {
      if (out_index != NULL) {
        *out_index = i;
      }

      return row;
    }
  }

  return NULL;
}

void layout_update(Layout *layout, const Document doc, bool wrap_enabled) {
  layout->rows = NULL;
  layout->row_count = 0;
  layout->row_capacity = 0;
  layout->hard_break_count = 0;

  layout->content_width = 0.0f;
  layout->content_height = 0.0f;
  arena_reset(layout->arena);
  layout->row_capacity = layout_estimate_row_count(*layout, doc, wrap_enabled);
  assert(layout->row_capacity != 0);
  layout->rows =
      ARENA_PUSH_ARRAY(layout->arena, layout->row_capacity, LayoutRow);

  f32 px = 0;
  f32 py = 0;
  u32 len = doc_get_length(doc);
  LayoutRow *current_row = &layout->rows[0];
  for (u32 i = 0; i < len; i++) {
    char c = doc_get_char(doc, i);
    float advance = layout_get_advance(*layout, c);

    bool hard_break = c == '\n';
    bool soft_break = px + advance > layout->wrap_width;
    if (hard_break || (wrap_enabled && soft_break)) {
      current_row->offset_end =
          i + 1 - hard_break; // only add one if soft but not hard break
      current_row->w = px;
      current_row->h = layout->line_height;
      current_row->break_kind = hard_break   ? BREAK_HARD
                                : soft_break ? BREAK_SOFT
                                             : BREAK_NONE; // impossible
      current_row->logical_line_index = layout->hard_break_count;
      layout->hard_break_count += hard_break;

      layout->content_width = max(layout->content_width, px);
      px = 0;
      py += layout->line_height;

      assert(layout->row_count < layout->row_capacity);
      current_row = &layout->rows[++layout->row_count];
      current_row->offset_start = i + 1;
      current_row->y = py;

      continue;
    }

    px += advance;
  }
  layout->content_height = py;
}

// TODO(melvil): maybe can be directly compute
void layout_get_visible_line(const Layout layout, const Viewport vp,
                             u32 *line_start, u32 *line_end) {
  *line_start = 0;
  *line_end = layout.row_count;

  for (u32 i = 0; i < layout.row_count; i++) {
    LayoutRow *row = &layout.rows[i];

    if (row->y + row->h < vp.scroll_y) {
      continue;
    }

    *line_start = i;
    break;
  }

  for (u32 i = *line_start; i < layout.row_count; i++) {
    LayoutRow *row = &layout.rows[i];

    if (row->y >= vp.scroll_y + vp.h) {
      *line_end = i;
      break;
    }
  }
}
#define layout_render(editor, atlas, list)                                     \
  layout_quad_list((editor).layout, (editor).doc, (editor).vp, (atlas),        \
                   (editor).color, (list))
void layout_quad_list(const Layout layout, const Document doc,
                      const Viewport vp, const Atlas a, const Vec4 color,
                      QuadInstanceList *list) {
  u32 line_start = 0, line_end = 0;
  layout_get_visible_line(layout, vp, &line_start, &line_end);
  for (u32 i = line_start; i < line_end; i++) {
    LayoutRow *row = &layout.rows[i];

    f32 px = vp.x + vp.scroll_x;
    f32 row_top = vp.y + row->y - vp.scroll_y;
    f32 baseline_y = row_top + a.ascent;
    for (u32 offset = row->offset_start; offset < row->offset_end; offset++) {
      char c = doc_get_char(doc, offset);
      assert(c != '\n');

      f32 advance = layout_get_advance(layout, c);

      if (c == ' ' || c == '\t') {
        px += advance;
        continue;
      }

      const stbtt_bakedchar *g = atlas_get_glyph((Atlas *)&a, c);
      f32 glyph_x = roundf(px + g->xoff);
      f32 glyph_y = roundf(baseline_y + g->yoff);
      // y culling should be already down -> visible line
      if (glyph_x + advance < vp.x || glyph_x > vp.x + vp.w) {
        px += advance;
        continue;
      }

      assert(list->capacity > list->length);

      // printf("%c", c);
      list->data[list->length++] = (QuadInstance){
          .x = glyph_x,
          .y = glyph_y,
          .w = (f32)(g->x1 - g->x0),
          .h = (f32)(g->y1 - g->y0),
          .u_min_x = (f32)g->x0 / (f32)a.w,
          .u_min_y = (f32)g->y0 / (f32)a.h,
          .u_max_x = (f32)g->x1 / (f32)a.w,
          .u_max_y = (f32)g->y1 / (f32)a.h,
          .r = color.R,
          .g = color.G,
          .b = color.B,
          .a = color.A,
      };

      px += advance;
    }
    // printf("\n");
  }
}

//
// @cursor
typedef enum CursorHorizontalMovementKind {
  MOVE_SIMPLE,
  MOVE_WORD,
  MOVE_LINE_START,
  MOVE_LINE_END,
} CursorHorizontalMovementKind;
void cursor_move_left(Cursor *cursor, const Document doc,
                      CursorHorizontalMovementKind movement_kind) {
  u32 offset = cursor->offset;
  switch (movement_kind) {
  case MOVE_SIMPLE:
    offset = doc_previous_offset(doc, offset);
    break;
  case MOVE_WORD: {
    // skip whitespace
    while (offset > 0) {
      CharacterClass current_class =
          character_class(doc_get_char(doc, offset - 1));
      if (current_class != CHARACTER_SPACE &&
          current_class != CHARACTER_LINE_END)
        break;
      offset = doc_previous_offset(doc, offset);
    }
    // skip current class
    if (offset > 0) {
      CharacterClass current_class =
          character_class(doc_get_char(doc, offset - 1));
      while (offset > 0 &&
             character_class(doc_get_char(doc, offset - 1)) == current_class) {
        offset = doc_previous_offset(doc, offset);
      }
    }
    break;
  }
  case MOVE_LINE_START:
    while (offset > 0 && doc_get_char(doc, offset - 1) != '\n')
      offset = doc_previous_offset(doc, offset);
    break;

  case MOVE_LINE_END:
    break;
  }

  cursor->offset = offset;
  cursor->prefered_col_valid = false;
}
void cursor_move_right(Cursor *cursor, const Document doc,
                       CursorHorizontalMovementKind movement_kind) {
  u32 offset = cursor->offset;
  u32 len = doc_get_length(doc);
  switch (movement_kind) {
  case MOVE_SIMPLE:
    offset = doc_next_offset(doc, offset);
    break;
  case MOVE_WORD: {
    // skip whitespace
    while (offset < len) {
      CharacterClass current_class = character_class(doc_get_char(doc, offset));
      if (current_class != CHARACTER_SPACE &&
          current_class != CHARACTER_LINE_END)
        break;
      offset = doc_next_offset(doc, offset);
    }
    // skip current class
    if (offset < len) {
      CharacterClass current_class = character_class(doc_get_char(doc, offset));
      while (offset < len &&
             character_class(doc_get_char(doc, offset)) == current_class) {
        offset = doc_next_offset(doc, offset);
      }
    }
    break;
  }
  case MOVE_LINE_END:
    while (offset < len && doc_get_char(doc, offset) != '\n')
      offset = doc_next_offset(doc, offset);
    break;
  case MOVE_LINE_START:
    break;
  }

  cursor->offset = offset;
  cursor->prefered_col_valid = false;
}
void cursor_move_down(Cursor *cursor, const Layout layout) {
  u32 row_index = 0;
  LayoutRow *current_row = layout_find_row(&layout, cursor->offset, &row_index);
  if (row_index >= layout.row_count - 1) {
    cursor->offset = current_row->offset_end;
    return;
  }
  u32 relative_offset;
  if (cursor->prefered_col_valid) {
    relative_offset = cursor->prefered_col;
  } else {
    relative_offset = cursor->offset - current_row->offset_start;
    cursor->prefered_col = relative_offset;
    cursor->prefered_col_valid = true;
  }

  LayoutRow *next_row = &layout.rows[row_index + 1];
  cursor->offset =
      min(next_row->offset_start + relative_offset, next_row->offset_end);
}
void cursor_move_up(Cursor *cursor, const Layout layout) {
  u32 row_index = 0;
  LayoutRow *current_row = layout_find_row(&layout, cursor->offset, &row_index);
  if (row_index == 0) {
    cursor->offset = current_row->offset_start;
    return;
  }
  u32 relative_offset;
  if (cursor->prefered_col_valid) {
    relative_offset = cursor->prefered_col;
  } else {
    relative_offset = cursor->offset - current_row->offset_start;
    cursor->prefered_col = relative_offset;
    cursor->prefered_col_valid = true;
  }

  LayoutRow *previous = &layout.rows[row_index - 1];
  cursor->offset =
      min(previous->offset_start + relative_offset, previous->offset_end);
}
void cursor_place(Cursor *cursor, const Layout *layout, const Document *doc,
                  f64 x, f64 y) {
  LayoutRow *row;
  for (u32 i = 0; i < layout->row_count; i++) {
    row = &layout->rows[i];
    if (row->y <= y && y <= row->y + layout->line_height) {
      break;
    }
  }
  u32 offset;
  f32 px = 0;
  for (offset = row->offset_start; offset < row->offset_end; offset++) {
    char c = doc_get_char(*doc, offset);
    f32 advance = layout_get_advance(*layout, c);
    if (px <= x && x <= px + advance) {
      break;
    }
    px += advance;
  }
  cursor->offset = offset;
}
static struct {
  Rect rect;
  LayoutRow *row;
  char c;
} debug_cursor = {0};

#define cursor_render(editor, atlas, color, list)                              \
  cursor_quad_list(&(editor).cursor, (editor).layout, (editor).doc,            \
                   (editor).vp, (atlas), (color), (list))
void cursor_quad_list(Cursor *cursor, const Layout layout, const Document doc,
                      const Viewport vp, const Atlas a, const Vec4 color,
                      QuadInstanceList *list) {
  LayoutRow *row = layout_find_row(&layout, cursor->offset, NULL);
  assert(cursor->offset >= row->offset_start &&
         cursor->offset <= row->offset_end);
  f32 x = 0;
  for (u32 i = row->offset_start; i < cursor->offset; i++) {
    char c = doc_get_char(doc, i);
    x += layout_get_advance(layout, c);
  }

  Rect cursor_rect = {.x = vp.x + x + vp.scroll_x,
                      .y = vp.y + row->y - vp.scroll_y,
                      .w = 2,
                      .h = a.ascent - a.descent};
  list->data[list->length++] = (QuadInstance){
      .x = cursor_rect.x,
      .y = cursor_rect.y,
      .w = cursor_rect.w,
      .h = cursor_rect.h,
      .u_min_x = a.white_x,
      .u_min_y = a.white_y,
      .u_max_x = a.white_x,
      .u_max_y = a.white_y,
      .r = color.R,
      .g = color.G,
      .b = color.B,
      .a = color.A,
  };
  debug_cursor.rect = cursor_rect;
  debug_cursor.row = row;
  debug_cursor.c = doc_get_char(doc, cursor->offset);
}

//
// @editor
void editor_set_scroll(Editor *editor, f32 scroll_x, f32 scroll_y) {
  if (editor->vp.w < editor->layout.content_width) {
    editor->vp.scroll_x =
        clamp(scroll_x, editor->vp.w - editor->layout.content_width, 0);
  } else {
    editor->vp.scroll_x = 0;
  }
  if (editor->vp.h < editor->layout.content_height) {
    editor->vp.scroll_y =
        clamp(scroll_y, 0, editor->layout.content_height - editor->vp.h);
  } else {
    editor->vp.scroll_y = 0;
  }
  // TODO(melvil): put somewhere else
  layout_update(&editor->layout, editor->doc, editor->wrap_enabled);
}
void editor_add_scroll(Editor *editor, f32 d_scroll_x, f32 d_scroll_y) {
  editor_set_scroll(editor, editor->vp.scroll_x + d_scroll_x,
                    editor->vp.scroll_y + d_scroll_y);
}

//
// @gutter
static u32 decimal_digit_count(u32 value) {
  u32 count = 1;

  while (value >= 10) {
    value /= 10;
    count++;
  }

  return count;
}
f32 gutter_get_width(const Layout *layout) {
  if (layout->row_count < 1)
    return 1;
  LayoutRow *last_row = &layout->rows[layout->row_count - 1];
  f32 max_width = (f32)decimal_digit_count(last_row->logical_line_index) *
                  layout->glyph_advance;
  return max_width;
}
#define gutter_render(app, rect, color)                                        \
  gutter_quad_list(&(app)->editor.gutter, rect, &(app)->editor.layout,         \
                   &(app)->editor.vp, &(app)->atlas, (color),                  \
                   &(app)->quad_list)
void gutter_quad_list(const Gutter *gutter, const Rect *rect,
                      const Layout *layout, const Viewport *vp, const Atlas *a,
                      const Vec4 *color, QuadInstanceList *list) {
  u32 line_start = 0, line_end = 0;
  layout_get_visible_line(*layout, *vp, &line_start, &line_end);
  switch (gutter->kind) {
  case GUTTER_NONE:
    return;
  case GUTTER_ABSOLUTE:
    u32 last_line_index = UINT32_MAX;
    // u32 digit_number =
    //     decimal_digit_count(layout->rows[layout->row_count].logical_line_index);
    for (u32 i = line_start; i < line_end; i++) {
      LayoutRow *row = &layout->rows[i];

      f32 px = rect->x;
      f32 row_top = rect->y + row->y - vp->scroll_y;
      // printf("line %u\n", row->logical_line_index);
      if (row->logical_line_index == last_line_index) {
        continue;
      }
      ArenaTemp temp = arena_temp_begin(layout->arena);
      last_line_index = row->logical_line_index;
      Str line_str = str_format(temp.arena, "%*u",
                                (u32)roundf(rect->w / layout->glyph_advance),
                                last_line_index + 1);

      // printf("%.*s\n", STR_FMT(line_str));
      // printf("%f\n", rect->w / layout->glyph_advance);
      render_command_execute(
          a,
          (RenderCommand){.kind = RENDER_COMMAND_TEXT,
                          .bounding_box = {.x = px,
                                           .y = row_top,
                                           .w = rect->w,
                                           .h = layout->line_height},
                          .text = {.str = line_str, .color = *color}},
          list);
      arena_temp_end(temp);
    }
    return;
  }
}

int compute_frame(Application *app) {
  app->quad_list.length = 0;

  Rect outer = (Rect){
      .x = MARGIN_X,
      .y = MARGIN_Y,
      .w = (f32)app->w - MARGIN_X * 2,
      .h = (f32)app->h - MARGIN_Y * 2 - 64,
  };

  Rect editor_viewport = {
      .x = outer.x + PADDING_X,
      .y = outer.y + PADDING_Y,
      .w = outer.w - PADDING_X * 2,
      .h = outer.h - PADDING_Y * 2,
  };

  Rect information_outer = {
      .x = outer.x,
      .y = outer.y + outer.h,
      .w = outer.w,
      .h = 64,
  };
  Rect information_inner = {
      .x = information_outer.x + 8,
      .y = information_outer.y + 8,
      .w = information_outer.w - 16,
      .h = information_outer.h - 16,
  };
  // rect
  {
    render_command_execute(
        &app->atlas,
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
    app->editor.wrap_enabled = false;
    app->editor.color = (Vec4){.R = 1., .G = 1., .B = 1., .A = 0.8f};
    layout_update(&app->editor.layout, app->editor.doc,
                  app->editor.wrap_enabled);
    app->editor.gutter.kind = GUTTER_ABSOLUTE;
    f32 gutter_width = gutter_get_width(&app->editor.layout);
    Rect gutter_rect = {
        .x = editor_viewport.x,
        .y = editor_viewport.y,
        .w = gutter_width,
        .h = editor_viewport.h,
    };
    Vec4 gutter_color = {.R = 1., .G = 1., .B = 1., .A = .5f};
    render_command_execute(
        &app->atlas,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .bounding_box =
                {
                    .x = gutter_rect.x,
                    .y = gutter_rect.y,
                    .w = gutter_rect.w,
                    .h = gutter_rect.h,
                },
            .rect = {.color = {.R = 0., .G = 0., .B = 1., .A = .05f}}},
        &app->quad_list);
    gutter_render(app, &gutter_rect, &gutter_color);
    app->editor.vp.x = editor_viewport.x + gutter_rect.w + 16;
    app->editor.vp.y = editor_viewport.y;
    app->editor.vp.w = editor_viewport.w - gutter_width - 16;
    app->editor.vp.h = editor_viewport.h;

    app->editor.wrap_enabled = true;
    app->editor.layout.wrap_width = editor_viewport.w;
    render_command_execute(
        &app->atlas,
        (RenderCommand){
            .kind = RENDER_COMMAND_RECT,
            .bounding_box =
                {
                    .x = app->editor.vp.x,
                    .y = app->editor.vp.y,
                    .w = app->editor.vp.w,
                    .h = app->editor.vp.h,
                },
            .rect = {.color = {.R = 0., .G = 1., .B = 0., .A = .05f}}},
        &app->quad_list);
    layout_render(app->editor, app->atlas, &app->quad_list);
    Vec4 cursor_color = {.R = 1., .G = 1., .B = 1., .A = 1.f};
    app->editor.cursor.affinity = CARET_AFFINITY_DOWNSTREAM;
    cursor_render(app->editor, app->atlas, cursor_color, &app->quad_list);
  }

  // cursor
  // Rect cursor_rect;
  // compute_cursor_rect(app, app->editor_cursor, app->editor_text,
  // &cursor_rect); render_command_execute(
  //     app,
  //     (RenderCommand){.kind = RENDER_COMMAND_RECT,
  //                     .bounding_box = cursor_rect,
  //                     .rect = {.color = {.R = 1., .G = 1., .B = 1., .A
  //                     = 1.}}},
  //     &app->quad_list);

  // @information
  Cursor *cursor = &app->editor.cursor;
  ArenaTemp temp = arena_temp_begin(app->scratch_arena);
  Str prefered_col_str =
      (cursor->prefered_col_valid)
          ? str_format(temp.arena, "pref_col=%u; ", cursor->prefered_col)
          : S("");
  // row
  Str information_str;
  Vec4 information_color;
  switch (info_mode) {
  case INFO_ROW:
    information_str = str_format(
        temp.arena,
        "ROW: x=%.0f; y=%.0f; offset=%u;%.*s --- row=%u-%u; char=%u\n",
        debug_cursor.rect.x, debug_cursor.rect.y, cursor->offset,
        STR_FMT(prefered_col_str), debug_cursor.row->offset_start,
        debug_cursor.row->offset_end, debug_cursor.c, app->fps);
    information_color = (Vec4){.R = 1., .G = 0., .B = 0., .A = 1.};
    break;
  case INFO_SCROLL:
    information_str = str_format(
        temp.arena,
        "SCROLL: x=%.0f; y=%.0f;"
        "MOUSE: x=%.0f; y=%.0f; "
        "(editor --> x=%.0f; y=%.0f)\n"
        "CONTENT: w=%.0f; h=%.0f;",
        app->editor.vp.scroll_x, app->editor.vp.scroll_y, mouse_x, mouse_y,
        editor_mouse_x(mouse_x), editor_mouse_y(mouse_y),
        app->editor.layout.content_width, app->editor.layout.content_height);
    information_color = (Vec4){.R = .5, .G = .5, .B = 0., .A = 1.};

    break;
  case INFO_GRAPHICS:
    information_str = str_format(temp.arena, "FPS: %f", app->fps);
    information_color = (Vec4){.R = 0., .G = .5, .B = 0.5, .A = 1.};
    break;
  }

  render_command_execute(&app->atlas,
                         (RenderCommand){.kind = RENDER_COMMAND_RECT,
                                         .bounding_box = information_outer,
                                         .rect = {.color = information_color}},
                         &app->quad_list);
  render_command_execute(
      &app->atlas,
      (RenderCommand){.kind = RENDER_COMMAND_TEXT,
                      .bounding_box = information_inner,
                      .text = {.str = information_str,
                               .color = {.R = 1., .G = 1., .B = 1., .A = 1.}}},
      &app->quad_list);
  arena_temp_end(temp);

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
  Editor *editor = &app->editor;
  bool trigger = action == GLFW_PRESS || action == GLFW_REPEAT;
  bool ctrl = mods & GLFW_MOD_CONTROL;
  if (!trigger)
    return;

  // arrow
  switch (key) {
  case GLFW_KEY_LEFT:
    if (ctrl)
      cursor_move_left(&editor->cursor, editor->doc, MOVE_WORD);
    else
      cursor_move_left(&editor->cursor, editor->doc, MOVE_SIMPLE);
    break;
  case GLFW_KEY_RIGHT:
    if (ctrl)
      cursor_move_right(&editor->cursor, editor->doc, MOVE_WORD);
    else
      cursor_move_right(&editor->cursor, editor->doc, MOVE_SIMPLE);
    break;
  case GLFW_KEY_UP:
    if (ctrl)
      editor_add_scroll(editor, 0, -editor->layout.line_height);
    else
      cursor_move_up(&editor->cursor, editor->layout);
    break;
  case GLFW_KEY_DOWN:
    if (ctrl)
      editor_add_scroll(editor, 0, +editor->layout.line_height);
    else
      cursor_move_down(&editor->cursor, editor->layout);
    break;
  case GLFW_KEY_HOME:
    if (ctrl)
      editor->cursor.offset = 0;
    else
      cursor_move_left(&editor->cursor, editor->doc, MOVE_LINE_START);
    break;
  case GLFW_KEY_END:
    if (ctrl)
      editor->cursor.offset = doc_get_length(editor->doc) - 1;
    else
      cursor_move_right(&editor->cursor, editor->doc, MOVE_LINE_END);
    break;
  }
  // ctrl key command
  if (ctrl) {
    switch (key) {
    case GLFW_KEY_0:
      if (ctrl) {
        app->editor.vp.scroll_x = 0;
        app->editor.vp.scroll_y = 0;
      }
      break;
    case GLFW_KEY_G:
      info_mode = INFO_GRAPHICS;
      break;
    case GLFW_KEY_R:
      info_mode = INFO_ROW;
      break;
    case GLFW_KEY_S:
      info_mode = INFO_SCROLL;
      break;
    }
  }
}

void on_char_input(Application *app, u32 c) {
  (void)app;
  printf("on_char %c\n", c);
}

#define SCROLL_FACTOR 20
void on_mouse(Application *app, const PlatMouseEvent *ev) {
  switch (ev->kind) {
  case PLAT_MOUSE_MOVE: /* ev->u.move.x, .y */
    mouse_x = ev->u.move.x;
    mouse_y = ev->u.move.y;
    break;
  case PLAT_MOUSE_BUTTON: /* ev->u.button.button/action/mods */
    if (ev->u.button.button == GLFW_MOUSE_BUTTON_1 &&
        ev->u.button.action == GLFW_PRESS) {
      if (app->editor.vp.x <= mouse_x &&
          mouse_x <= app->editor.vp.x + app->editor.vp.w &&
          app->editor.vp.y <= mouse_y &&
          mouse_y <= app->editor.vp.y + app->editor.vp.h) {
        f64 x = editor_mouse_x(mouse_x);
        f64 y = editor_mouse_y(mouse_y);
        cursor_place(&app->editor.cursor, &app->editor.layout, &app->editor.doc,
                     x, y);
      }
    }
    break;
  case PLAT_MOUSE_SCROLL:
    editor_add_scroll(&app->editor, (f32)ev->u.scroll.xoff * SCROLL_FACTOR,
                      -(f32)ev->u.scroll.yoff * SCROLL_FACTOR);
    break;
  case PLAT_MOUSE_ENTER: /* ev->u.enter.entered */
    break;
  }
}

void update(Application *app) {
  draw_frame(app);
  compute_frame(app);
}
