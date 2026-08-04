#ifndef RELOAD_H
#define RELOAD_H

#include <dlfcn.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "lib/api.h"

typedef LibAPI (*LibGetApiFn)(void);

typedef struct Reload {
  const char *path;
  void       *handle;
  LibAPI      api;
  time_t      last_mtime;

  void (*before_unload)(Application *app);
  void (*after_load)(Application *app);
  Application *app;
} Reload;

static inline bool reload_open(Reload *r) {
  r->handle = dlopen(r->path, RTLD_NOW);
  if (!r->handle) {
    fprintf(stderr, "reload: dlopen failed: %s\n", dlerror());
    return false;
  }

  LibGetApiFn get_api;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
  get_api = (LibGetApiFn)dlsym(r->handle, "lib_get_api");
#pragma GCC diagnostic pop
  if (!get_api) {
    fprintf(stderr, "reload: lib_get_api missing: %s\n", dlerror());
    dlclose(r->handle);
    r->handle = NULL;
    return false;
  }
  r->api = get_api();

  struct stat st;
  if (stat(r->path, &st) == 0) r->last_mtime = st.st_mtime;

  if (r->after_load) r->after_load(r->app);
  r->api.load(r->app);
  return true;
}

static inline bool reload_poll(Reload *r) {
  struct stat st;
  if (stat(r->path, &st) != 0) return false;
  if (st.st_mtime == r->last_mtime) return false;

  if (r->before_unload) r->before_unload(r->app);
  r->api.unload(r->app);

  dlclose(r->handle);
  r->handle = NULL;
  r->last_mtime = st.st_mtime;

  // race: gcc may still be flushing the .so
  usleep(50000);

  printf("reload: detected change, reloading %s\n", r->path);
  return reload_open(r);
}

static inline void reload_close(Reload *r) {
  if (r->handle) {
    if (r->before_unload) r->before_unload(r->app);
    r->api.unload(r->app);
    dlclose(r->handle);
    r->handle = NULL;
  }
}

#endif
