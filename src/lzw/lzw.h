#pragma once
#include "config.h"
#include "common.h"
#include "io_func.h"

typedef struct
{
    uint8_t **dict;
    size_t dict_size;
    int dict_i;
    uint8_t code_len;
} lzw_t;

void lzw_init(lzw_t *lzw);
void lzw_reset(lzw_t *lzw);
int lzw_compress(lzw_t *lzw, read_func_t src_r, write_func_t dst_w, void *ctx);
int lzw_decompress(lzw_t *lzw, read_func_t src_r,
    write_func_t dst_w, void *ctx);
void lzw_deinit(lzw_t *lzw);
