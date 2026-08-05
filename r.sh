#!/usr/bin/env bash
set -e

SRC_DIR="./src"
BUILD_DIR="./build"
HOST_EXEC="app"
LIB_NAME="libapp.so"

COMMON_LIBS="-lm -lglfw -lvulkan"

SUBCMD="${1:-all}"
MODE="${2:-debug}"

cflags_for_mode() {
  case "$1" in
    debug)
      echo "-g3 -O0 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -DDEBUG -DVK_ENABLE_VALIDATION"
      ;;
    debug-sanitize)
      echo "-g3 -O0 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -fanalyzer -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG -DVK_ENABLE_VALIDATION"
      ;;
    release)
      echo "-O2 -DNDEBUG"
      ;;
    *)
      printf 'unknown mode: %s\n' "$1" >&2
      return 1
      ;;
  esac
}

build_shaders() {
  mkdir -p "$BUILD_DIR/shaders"
  local count=0
  for shader in "$SRC_DIR"/shaders/*.slang; do
    [ -e "$shader" ] || continue
    out="$BUILD_DIR/shaders/$(basename "${shader%.slang}").spv"
    slangc "$shader" -target spirv -profile spirv_1_4 \
      -emit-spirv-directly -fvk-use-entrypoint-name \
      -entry vertMain -entry fragMain -o "$out"
    count=$((count + 1))
  done
  printf '[shaders] compiled %d shader(s)\n' "$count"
}

build_host() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local t0=$SECONDS
  gcc $cflags -rdynamic "$SRC_DIR/host/main.c" -o "$BUILD_DIR/$HOST_EXEC" \
    $COMMON_LIBS -ldl -I"$SRC_DIR" -Iexternal
  printf '[host]    built %s (%s, %ds)\n' "$HOST_EXEC" "$MODE" "$((SECONDS - t0))"
}

build_lib() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local t0=$SECONDS
  gcc $cflags -shared -fPIC "$SRC_DIR/lib/main.c" -o "$BUILD_DIR/$LIB_NAME" \
    $COMMON_LIBS -I"$SRC_DIR" -Iexternal
  printf '[lib]     built %s (%s, %ds)\n' "$LIB_NAME" "$MODE" "$((SECONDS - t0))"
}

run_host() {
  printf '[run]     %s\n' "$HOST_EXEC"
  if [ "$MODE" = "debug-sanitize" ]; then
    ASAN_OPTIONS=detect_leaks=0 "$BUILD_DIR/$HOST_EXEC"
  else
    "$BUILD_DIR/$HOST_EXEC"
  fi
}

mkdir -p "$BUILD_DIR"

case "$SUBCMD" in
  all)
    # full: shaders + host + lib + run
    build_shaders
    build_host
    build_lib
    run_host
    ;;
  build)
    # full build, no run
    build_shaders
    build_host
    build_lib
    ;;
  host)
    # host only (no run)
    build_host
    ;;
  lib)
    # lib only — hot reload target
    build_lib
    ;;
  run)
    # run existing binary
    run_host
    ;;
  shaders)
    build_shaders
    ;;
  *)
    cat <<EOF >&2
Usage: $0 <command> [mode]

Commands:
  all       shaders + host + lib + run  (default)
  build     shaders + host + lib, no run
  host      host only, no run
  lib       lib only (hot reload)
  run       run existing binary
  shaders   recompile shaders only

Modes:
  debug            (default)
  debug-sanitize
  release

Examples:
  $0                    # full debug build + run
  $0 lib                # recompile lib only (hot reload)
  $0 build release      # full release build, no run
EOF
    exit 1
    ;;
esac
