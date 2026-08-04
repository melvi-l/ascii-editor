#ifndef API_H
#define API_H

#include "common/common.h"

typedef struct {
  void (*load)(Application *app);
  void (*unload)(Application *app);
  void (*update)(Application *app);
  void (*resize)(Application *app, u32 w, u32 h);
  void (*key)(Application *app, i32 key, i32 scancode, i32 action, i32 mods);
  void (*char_input)(Application *app, u32 c);
} LibAPI;

#endif // API_H
