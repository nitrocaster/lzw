#pragma once
#include "config.h"
#include "common.h"
#include "io_func.h"

typedef struct
{
    size_t (*cb)(void *ctx, uint8_t *buf, size_t count);
    void *ctx;
    uint8_t buf;
    int buf_pos;
    uint32_t mask;
} bitstream_t;

void bitstream_init_r(bitstream_t *bs, read_func_t r, void *ctx);
void bitstream_init_w(bitstream_t *bs, write_func_t w, void *ctx);
void bitstream_write(bitstream_t *bs, int code, uint8_t code_len);
int bitstream_read(bitstream_t *bs);
