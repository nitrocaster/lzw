#pragma once
#include "config.h"
#include "common.h"

typedef size_t(*read_func_t)(void *ctx, uint8_t *dst, size_t count);
typedef size_t(*write_func_t)(void *ctx, uint8_t *src, size_t count);
