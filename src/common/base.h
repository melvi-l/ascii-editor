#ifndef BASE_H
#define BASE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) < (b)) ? (b) : (a))
#define sign(i) ((i) > 0) - ((i) < 0)
#define clamp(x, lo, hi) (max((lo), min((x), (hi))))

#define B_FMT(b) ((b)) ? "true" : "false"

#define ll_push(f, l, n)                                                       \
  ((f) == 0) ? ((f) = (l) = (n), (n)->next = 0)                                \
             : ((l)->next = (n), (l) = (n), (n)->next = 0)

#define KiB(n) ((u64)(n) << 10)
#define MiB(n) ((u64)(n) << 20)
#define GiB(n) ((u64)(n) << 30)

// @log
#ifndef LOG_MODULE
#define LOG_MODULE "GLOBAL"
#endif

#define LOG(fmt, ...) printf("[" LOG_MODULE "] " fmt "\n", ##__VA_ARGS__)

#define TIME_BLOCK(label, expr)                                                \
  do {                                                                         \
    double _t = glfwGetTime();                                                 \
    expr;                                                                      \
    printf("%s: %.3f ms\n", label, (glfwGetTime() - _t) * 1000.0);             \
  } while (0)

// @arena
#define ARENA_DEFAULT_BLOCK_SIZE KiB(4)
#define ARENA_DEFAULT_ALIGNMENT 8

typedef struct _ArenaBlock {
  struct _ArenaBlock *next;
  size_t size;
  size_t used;
  u8 data[];
} ArenaBlock;

typedef struct {
  char *debug_name;
  ArenaBlock *first;
  ArenaBlock *current;
  size_t block_size;
  size_t total_allocated;
  size_t total_used;
} Arena;

static inline size_t align_up(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

static inline ArenaBlock *arena_block_create(size_t min_size) {
  ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + min_size);
  if (!block) {
    perror("arena block creation");
    abort();
  }
  block->size = min_size;
  block->used = 0;
  block->next = NULL;
  return block;
}

Arena *arena_create(size_t block_size);

void arena_destroy(Arena *arena);

void *arena_alloc(Arena *arena, size_t size);

void *arena_alloc_zero(Arena *arena, size_t count, size_t size);

// Reset arena for reuse (keeps memory)
void arena_reset(Arena *arena);

// Free all blocks except the first
void arena_clear(Arena *arena);

typedef struct {
  Arena *arena;
  ArenaBlock *saved_block;
  size_t saved_used;
} ArenaTemp;

ArenaTemp arena_temp_begin(Arena *arena);
void arena_temp_end(ArenaTemp temp);

static inline bool arena_try_stitch(Arena *arena, void *ptr, u64 size,
                                    u64 grow) {
  ArenaBlock *block = arena->current;

  u8 *ptr_end = (u8 *)ptr + size;

  u8 *block_end = block->data + block->used;

  if (ptr_end != block_end)
    return false;

  if (block->used + grow > block->size)
    return false;

  block->used += grow;
  arena->total_used += grow;

  return true;
}

#define ARENA_PUSH_STRUCT(arena, type)                                         \
  (type *)arena_alloc_zero(arena, 1, sizeof(type))
#define ARENA_PUSH_ARRAY(arena, count, type)                                   \
  (type *)arena_alloc_zero(arena, count, sizeof(type))

// @str
typedef struct {
  u8 *data;
  u64 length;
} Str;

static inline Str S(const char *cstr) {
  return (Str){(u8 *)cstr, (u64)strlen(cstr)};
}
static inline Str S_line(const char *cstr) {
  i32 len = (i32)strlen(cstr);
  if (cstr[len - 1] == '\n') {
    len--;
  }
  return (Str){(u8 *)cstr, (u64)len};
}

static inline void str_to_cstr(Str str, char *out) {
  for (u64 i = 0; i < str.length; i++) {
    *out = (char)str.data[i];
    out++;
  }
  *out = 0;
}
static inline Str str_from(Str base, i32 start) {
  if (start < 0)
    start = 0;
  if (start > (i32)base.length)
    start = (i32)base.length;
  return (Str){base.data + start, base.length - (u64)start};
}

static inline Str str_from_to(Str base, i32 start, i32 end) {
  if (start < 0)
    start = 0;
  if (start > (i32)base.length)
    start = (i32)base.length;

  if (end < 0)
    end = (i32)(base.length + (u64)end);
  if (end < 0)
    end = 0;
  if (end > (i32)base.length)
    end = (i32)base.length;

  if (end < start)
    end = start;

  return (Str){base.data + start, (u64)(end - start)};
}

__attribute__((unused)) static Str str_copy(Arena *arena, const char *cstr) {
  assert(arena);

  u64 len = (u64)strlen(cstr);
  u8 *dst = ARENA_PUSH_ARRAY(arena, len, u8);
  if (len)
    memcpy(dst, cstr, (size_t)len);
  return (Str){dst, len};
}

__attribute__((unused)) static Str str_copy_str(Arena *arena, Str s) {
  assert(arena);

  u8 *dst = ARENA_PUSH_ARRAY(arena, s.length, u8);
  if (s.length)
    memcpy(dst, s.data, (size_t)s.length);
  return (Str){dst, s.length};
}

__attribute__((unused)) static Str str_format(Arena *arena, const char *fmt,
                                              ...) {
  va_list ap;

  va_start(ap, fmt);
  int needed = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  if (needed < 0) {
    // maybe return empty str
    abort();
  }

  u8 *cstr = ARENA_PUSH_ARRAY(arena, (size_t)needed + 1, u8);

  va_start(ap, fmt);
  int written = vsnprintf((char *)cstr, (size_t)needed + 1, fmt, ap);
  va_end(ap);

  if (written != needed) {
    // if not abort -> pop cstr from arena
    abort();
  }

  return (Str){cstr, (u64)written};
}

static inline bool str_cmp(Str a, Str b) {
  return ((a.length == b.length) && memcmp(a.data, b.data, a.length) == 0);
}

static inline bool str_cmp_cstr(Str a, const char *b) {
  return str_cmp(a, S(b));
}

#define STR_FMT(str) (int)((str).length), (str).data
#define STR_DEBUG(s)                                                           \
  printf("len: %lu -- content: %.*s\n", (s).length, STR_FMT(s))
#define STR_RAYFMT(str) TextSubtext((char *)(str.data), 0, (int)(str.length))

// @strbuilder
typedef struct {
  Str str;
  u64 capacity;
} StrBuilder;

static inline void str_builder_grow(Arena *arena, StrBuilder *builder,
                                    u64 new_capacity) {
  u64 grow = new_capacity - builder->capacity;
  if (arena_try_stitch(arena, builder->str.data, builder->capacity, grow)) {
    builder->capacity += grow;
    return;
  }

  u8 *new_value = ARENA_PUSH_ARRAY(arena, builder->capacity + grow, u8);
  memcpy(new_value, builder->str.data, builder->str.length);
  builder->str.data = new_value;
  builder->capacity += grow;
}

static inline Str str_builder_push(Arena *arena, StrBuilder *builder, Str str) {
  u64 length = builder->str.length + str.length;

  if (length > builder->capacity) {
    str_builder_grow(arena, builder, (builder->capacity + str.length) * 2);
  }

  memcpy(builder->str.data + builder->str.length, str.data, str.length);
  builder->str.length = length;
  return builder->str;
}

#define str_builder_push_data(arena, builder, data, size)                      \
  str_builder_push(arena, builder, (Str){(u8 *)data, (u64)size})

#define str_builder_push_array(arena, builder, ptr, count)                     \
  str_builder_push_data(arena, builder, ptr, sizeof(*(ptr)) * (count))

#define str_builder_push_struct(arena, builder, ptr)                           \
  str_builder_push_array(arena, builder, ptr, 1)

static inline void str_print_hex(Str *str) {
  for (u64 i = 0; i < str->length; i += 16) {
    printf("%08llX: ", (unsigned long long)i);

    for (u64 j = 0; j < 16; j++) {
      if (i + j < str->length)
        printf("%02X ", str->data[i + j]);
      else
        printf("   ");
    }

    printf("\n");
  }
}
#define ANSI_RESET "\x1b[0m"
#define ANSI_GRAY "\x1b[90m"
#define ANSI_HIGHLIGHT "\x1b[41;97m"
void str_builder_hexdump_at(Str *str, u64 pos);

static inline Str str_builder_push_str(Arena *arena, StrBuilder *builder,
                                       Str *str) {
  str_builder_push_struct(arena, builder, &str->length);
  return str_builder_push(arena, builder, *str);
}

static inline void str_builder_print_char(StrBuilder *builder) {
  for (u64 i = 0; i < builder->str.length; i += 16) {
    for (u64 j = 0; j < 16 && i + j < builder->str.length; j++) {
      u8 c = builder->str.data[i + j];
      printf("%c", (c >= 32 && c < 127) ? c : '.');
    }
    printf("\n");
  }
}

// @deserialized
static inline u64 str_deserialized(Str str, u64 off, void *read_dst,
                                   u64 read_size, u64 granularity) {
  u64 byte_left = str.length - min(off, str.length);
  u64 actually_readable_size = min(read_size, byte_left);
  u64 legally_readable_size =
      actually_readable_size - actually_readable_size % granularity;
  if (legally_readable_size > 0) {
    memcpy(read_dst, str.data + off, legally_readable_size);
  }
  return legally_readable_size;
}

#define str_deserialized_array(string, off, ptr, count)                        \
  str_deserialized((string), (off), (ptr), sizeof(*(ptr)) * (count),           \
                   sizeof(*(ptr)))

#define str_deserialized_struct(string, off, ptr)                              \
  str_deserialized_array(string, off, ptr, 1)

static inline u64 str_deserialized_str(Arena *arena, Str string, u64 off,
                                       Str *str) {
  u64 _off = off;
  _off += str_deserialized_struct(string, _off, &str->length);
  str->data = ARENA_PUSH_ARRAY(arena, str->length, u8);
  _off += str_deserialized_array(string, _off, str->data, str->length);
  return _off - off;
}

// @ring
static inline u64 read_ring(u8 *ring, u64 ring_position, u64 ring_size,
                            u8 *data, u64 size) {
  u64 ring_offset = ring_position % ring_size;
  u64 count_before_split = min(size, ring_size - ring_offset);
  u64 count_after_split = size - count_before_split;
  memcpy(data, ring + ring_offset, count_before_split);
  memcpy(data + count_before_split, ring, count_after_split);
  return size;
}
#define read_ring_struct(ring, ring_position, ring_size, ptr)                  \
  read_ring(ring, ring_position, ring_size, ptr, sizeof(ptr))
static inline u64 write_ring(u8 *ring, u64 ring_position, u64 ring_size,
                             u8 *data, u64 size) {
  assert(size <= ring_size);
  u64 ring_offset = ring_position % ring_size;
  u64 count_before_split = min(size, ring_size - ring_offset);
  u64 count_after_split = size - count_before_split;
  memcpy(ring + ring_offset, data, count_before_split);
  memcpy(ring, data + count_before_split, count_after_split);
  return size;
}
#define write_ring_struct(ring, ring_position, ring_size, ptr)                 \
  write_ring(ring, ring_position, ring_size, ptr, sizeof(ptr))

// @list
#define DECLARE_LIST_TYPE(T, Prefix)                                           \
                                                                               \
  typedef struct Prefix##Node {                                                \
    struct Prefix##Node *next;                                                 \
    T value;                                                                   \
  } Prefix##Node;                                                              \
                                                                               \
  typedef struct {                                                             \
    Prefix##Node *first;                                                       \
    Prefix##Node *last;                                                        \
    u64 count;                                                                 \
  } Prefix##List;                                                              \
                                                                               \
  static inline T *Prefix##List_push(Arena *arena, Prefix##List *list) {       \
    Prefix##Node *node = ARENA_PUSH_STRUCT(arena, Prefix##Node);               \
    node->next = NULL;                                                         \
                                                                               \
    if (list->last) {                                                          \
      list->last->next = node;                                                 \
    } else {                                                                   \
      list->first = node;                                                      \
    }                                                                          \
                                                                               \
    list->last = node;                                                         \
    list->count++;                                                             \
                                                                               \
    return &node->value;                                                       \
  }

f64 now_milliseconds();

#ifdef BASE_IMPLEMENTATION

Arena *arena_create(size_t block_size) {
  Arena *arena = malloc(sizeof(Arena));
  if (!arena) {
    perror("arena creation");
    abort();
  }

  ArenaBlock *block = arena_block_create(block_size);

  arena->first = block;
  arena->current = block;
  arena->block_size = block_size;
  arena->total_allocated = block_size;
  arena->total_used = 0;

  return arena;
}

void arena_destroy(Arena *arena) {
  if (!arena) {
    perror("arena destroy");
    abort();
  }
  ArenaBlock *block = arena->first;
  while (block) {
    ArenaBlock *next = block->next;
    free(block);
    block = next;
  }

  free(arena);
}

void *arena_alloc(Arena *arena, size_t size) {
  if (!arena) {
    fprintf(stderr, "arena passed is NULL\n");
    abort();
  }
  if (size == 0) {
    LOG("trying to allocate 0 bytes??");
    return NULL;
  }
  ArenaBlock *block = arena->current;

  size_t align_used = align_up(block->used, ARENA_DEFAULT_ALIGNMENT);
  size_t new_end = align_used + size;
  if (new_end <= block->size) {
    void *ptr = block->data + align_used;
    arena->total_used += new_end - block->used;
    block->used = new_end;
    return ptr;
  }

  size_t required_size = align_up(size, ARENA_DEFAULT_ALIGNMENT);
  ArenaBlock *new_block;
  if (block->next && block->next->size >= required_size) {
    new_block = block->next;
  } else {
    // need a new block
    size_t new_block_size = required_size;
    if (new_block_size <= arena->block_size) {
      new_block_size = arena->block_size;
    }
    new_block = arena_block_create(new_block_size);
    new_block->next = block->next;
    block->next = new_block;
    arena->total_allocated += new_block->size;
  }

  arena->current = new_block;

  void *ptr = new_block->data;
  new_block->used += size;
  arena->total_used += size;
  return ptr;
}

void *arena_alloc_zero(Arena *arena, size_t count, size_t size) {
  size_t total = count * size;
  void *ptr = arena_alloc(arena, total);
  if (ptr) {
    memset(ptr, 0, total);
  }
  return ptr;
}

void arena_reset(Arena *arena) {
  if (!arena) {
    fprintf(stderr, "Try to reset NULL arena\n");
    return;
  }

  ArenaBlock *b = arena->first;
  while (b) {
    b->used = 0;
    b = b->next;
  }

  arena->total_used = 0;
  arena->current = arena->first;
}

void arena_clear(Arena *arena) {
  if (!arena) {
    fprintf(stderr, "Try to reset NULL arena\n");
    return;
  }

  ArenaBlock *b = arena->first->next;
  while (b) {
    ArenaBlock *next = b->next;
    free(b);
    b = next;
  }

  arena->first->used = 0;
  arena->first->next = NULL;

  arena->total_used = 0;
  arena->current = arena->first;
  arena->total_allocated = arena->first->size;
}

ArenaTemp arena_temp_begin(Arena *arena) {
  return (ArenaTemp){.arena = arena,
                     .saved_block = arena->current,
                     .saved_used = arena->current->used};
}

void arena_temp_end(ArenaTemp temp) {
  Arena *a = temp.arena;

  ArenaBlock *b = temp.saved_block->next;
  while (b) {
    a->total_used -= b->used;
    b->used = 0;
    b = b->next;
  }

  a->total_used -= (temp.saved_block->used - temp.saved_used);
  temp.saved_block->used = temp.saved_used;
  a->current = temp.saved_block;
}

void str_builder_hexdump_at(Str *str, u64 pos) {
  for (u64 i = 0; i < str->length; i += 16) {
    printf("%08llX: ", (unsigned long long)i);

    for (u64 j = 0; j < 16; j++) {
      u64 idx = i + j;

      if (idx < str->length) {
        if (idx < pos) {
          printf(ANSI_GRAY "%02X " ANSI_RESET, str->data[idx]);
        } else if (idx == pos) {
          printf(ANSI_HIGHLIGHT "%02X " ANSI_RESET, str->data[idx]);
        } else {
          printf("%02X ", str->data[idx]);
        }
      } else {
        printf("   ");
      }
    }

    printf("\n");
  }
}

f64 now_milliseconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (f64)ts.tv_sec * 1000.0 + (f64)ts.tv_nsec / 1000000.0;
}

// @file
bool read_file(Arena *arena, Str path, Str *out) {
  char cstr[path.length + 1];
  str_to_cstr(path, cstr);
  FILE *file = fopen(cstr, "rb");

  if (file == NULL) {
    fprintf(stderr, "Failed to open shader file %*s\n", STR_FMT(path));
    return false;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fprintf(stderr, "Failed to seek shader file %*s\n", STR_FMT(path));
    fclose(file);
    return false;
  }

  long file_size = ftell(file);

  if (file_size < 0) {
    fprintf(stderr, "Failed to get shader file size: %*s\n", STR_FMT(path));
    fclose(file);
    return false;
  }

  rewind(file);

  u8 *buffer = ARENA_PUSH_ARRAY(arena, (size_t)file_size, u8);

  size_t read_size = fread(buffer, 1, (size_t)file_size, file);

  fclose(file);

  if (read_size != (size_t)file_size) {
    fprintf(stderr, "Failed to read shader file: %*s\n", STR_FMT(path));
    return false;
  }

  out->length = (u64)file_size;
  out->data = buffer;

  return true;
}

// @timer
typedef struct {
  f64 last_ms;
  f64 interval_ms;
} Timer;

bool timer_tick(Timer *timer) {
  f64 now_ms = now_milliseconds();

  if (timer->last_ms == 0.0) {
    timer->last_ms = now_ms;
    return false;
  }

  if (now_ms - timer->last_ms >= timer->interval_ms) {
    timer->last_ms = now_ms;
    return true;
  }

  return false;
}

#endif // BASE_IMPLEMENTATION
#endif // BASE_H
