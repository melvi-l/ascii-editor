#include "common/common.h"
#include "lib/api.h"

#include "lib/app.c"

// @lib api
void lib_load(Application *app) {
  rd_create_pipeline(app);
  compute_frame(app);
}

void lib_unload(Application *app) {
  if (app->pipeline != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(app->device);
    vkDestroyPipeline(app->device, app->pipeline, NULL);
    app->pipeline = VK_NULL_HANDLE;
  }
  app->editor_quad_is_dirty = false;
  arena_reset(app->scratch_arena);
}

void lib_update(Application *app) { update(app); }

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
