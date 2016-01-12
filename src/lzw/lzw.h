#pragma once
#include "config.h"
#include "common.h"
#include "io_func.h"

typedef struct
{
    uint16_t size;
    uint8_t *data;
} lzw_dict_entry_t;

typedef struct
{
    lzw_dict_entry_t *dict;
    size_t dict_size;
    size_t dict_capacity;
    int dict_i;
    uint8_t code_len;
} lzw_t;

void lzw_init(lzw_t *lzw);
void lzw_reset(lzw_t *lzw);
int lzw_compress(lzw_t *lzw, read_func_t src_r, write_func_t dst_w, void *ctx);
int lzw_decompress(lzw_t *lzw, read_func_t src_r,
    write_func_t dst_w, void *ctx);
void lzw_deinit(lzw_t *lzw);
