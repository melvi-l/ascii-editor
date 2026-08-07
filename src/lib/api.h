#ifndef API_H
#define API_H

#include "common/common.h"

typedef struct {
  void (*load)(Application *app);
  void (*unload)(Application *app);
  void (*update)(Application *app);
  void (*on_resize)(Application *app, u32 w, u32 h);
  void (*on_key)(Application *app, i32 key, i32 scancode, i32 action, i32 mods);
  void (*on_char)(Application *app, u32 c);
  void (*on_mouse)(Application *app, const PlatMouseEvent *ev);
} LibAPI;

#endif // API_H
