#include "common/common.h"
#include "lib/api.h"

#include "lib/app.c"

// @lib api
void lib_load(Application *app) {
  rd_create_pipeline(app);
  compute_frame(app);
}

void lib_unload(Application *app) {
  // mark quads dirty so next load recomputes from scratch
  app->editor_quad_is_dirty = false;
  // null out scratch arena temp state by resetting (safe: no Vulkan resources
  // here)
  arena_reset(app->scratch_arena);
}

void lib_update(Application *app) {
  compute_frame(app);
  draw_frame(app);
}

LibAPI lib_get_api(void) {
  return (LibAPI){
      .load = lib_load,
      .unload = lib_unload,
      .update = lib_update,
      .resize = on_resize,
      .key = on_key,
      .char_input = on_char_input,
  };
}
