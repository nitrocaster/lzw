#include "config.h"
#include "common.h"
#include "lzw.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

#define LZW_BUFFER_SIZE 256
#define LZW_INIT_CODE_LEN 9

typedef struct
{
    size_t (*cb)(void *ctx, uint8_t *buf, size_t count);
    void *ctx;
    uint8_t buf;
    int buf_pos;
    uint32_t mask;
} bitstream_t;

static void bitstream_init_r(bitstream_t *bs, read_func_t r, void *ctx)
{
    bs->cb = r;
    bs->ctx = ctx;
    bs->buf = 0;
    bs->mask = 256;
}

static void bitstream_init_w(bitstream_t *bs, write_func_t w, void *ctx)
{
    bs->cb = w;
    bs->ctx = ctx;
    bs->buf = 0;
    bs->buf_pos = 0;
}

static void bitstream_write(bitstream_t *bs, int code, uint8_t code_len)
{
    int mask_pos = 0;
    while (code_len--)
    {
        // right shift by bytes
        bs->buf |= ((code & (1 << mask_pos)) ? 1 : 0) << bs->buf_pos;
        mask_pos++;
        if (bs->buf_pos++ == 7)
        {
            bs->cb(bs->ctx, &bs->buf, 1);
            bs->buf = 0;
            bs->buf_pos = 0;
        }
    }
}

static int bitstream_read(bitstream_t *bs)
{
    if (bs->mask==256)
    {
        bs->mask = 1;
        if (!bs->cb(bs->ctx, &bs->buf, 1))
            return -1;
    }
    int bit = bs->buf & bs->mask ? 1 : 0;
    bs->mask <<= 1;
    return bit;
}

void lzw_init(lzw_t *lzw)
{
    uint8_t code_len = LZW_INIT_CODE_LEN;
    lzw->dict_size = (1 << code_len)*sizeof(uint8_t *);
    lzw->dict = malloc(lzw->dict_size);
    // first 256 entries are initialized with their indices
    for (int i = 0; i<256; i++)
    {
        lzw->dict[i] = malloc(2);
        lzw->dict[i][0] = i;
        lzw->dict[i][1] = 0;
    }
    lzw->dict[256] = 0; // 'reset' code
    lzw_reset(lzw);
}

void lzw_reset(lzw_t *lzw)
{
    lzw->dict_i = 257;
    lzw->code_len = LZW_INIT_CODE_LEN;
}

int lzw_compress(lzw_t *lzw, read_func_t src_r, write_func_t dst_w, void *ctx)
{
    bitstream_t bs_w;
    bitstream_init_w(&bs_w, dst_w, ctx);
    dst_w(ctx, &lzw->code_len, 1);
    size_t req_size = (1 << lzw->code_len)*sizeof(uint8_t *);
    if (req_size > lzw->dict_size)
    {
        lzw->dict_size = req_size;
        lzw->dict = realloc(lzw->dict, lzw->dict_size);
    }
    char buf[LZW_BUFFER_SIZE*2];
    char *inp = buf;
    size_t r_bytes = src_r(ctx, buf, LZW_BUFFER_SIZE*2);
    while (inp<=buf+r_bytes)
    {
        // advance buffer
        if (inp-buf>LZW_BUFFER_SIZE)
        {
            memcpy(buf, inp, r_bytes-(inp-buf));
            r_bytes -= inp-buf;
            r_bytes += src_r(ctx, buf+r_bytes,
                LZW_BUFFER_SIZE+(LZW_BUFFER_SIZE-r_bytes));
            inp = buf;
        }
        // find longest inp match in the dictionary
        int outc = -1;
        for (int i = lzw->dict_i-1; i; i--)
        {
            if (lzw->dict[i] &&
                !strncmp(lzw->dict[i], inp, strlen(lzw->dict[i])))
            {
                outc = i;
                break;
            }
        }
        assert(outc>=0);
        bitstream_write(&bs_w, outc, lzw->code_len);
        inp += strlen(lzw->dict[outc]);
        // expand the dictionary if necessary
        if (lzw->dict_i==(1 << lzw->code_len))
        {
            lzw->code_len++;
            lzw->dict_size = (1 << lzw->code_len)*sizeof(uint8_t *);
            lzw->dict = realloc(lzw->dict, lzw->dict_size);
        }
        // add this match, along with the next character, to the dictionary
        lzw->dict[lzw->dict_i] = malloc(strlen(lzw->dict[outc])+2);
        sprintf(lzw->dict[lzw->dict_i], "%s%c", lzw->dict[outc], *inp);
        lzw->dict_i++;
    }
    return 0;
}

int lzw_decompress(lzw_t *lzw, read_func_t src_r,
    write_func_t dst_w, void *ctx)
{   
    bitstream_t bs_r;
    bitstream_init_r(&bs_r, src_r, ctx);
    uint8_t init_code_len;
    src_r(ctx, &init_code_len, 1);
    if (init_code_len>=32)
        return 1;
    lzw->code_len = init_code_len;
    size_t req_size = (1 << lzw->code_len)*sizeof(uint8_t *);
    if (req_size > lzw->dict_size)
    {
        lzw->dict_size = req_size;
        lzw->dict = realloc(lzw->dict, lzw->dict_size);
    }
    int prev = -1;
    int done = 0;
    while (1)
    {
        int code = 0;
        for (int i = 0; i < lzw->code_len; i++)
        {
            int bit = bitstream_read(&bs_r);
            if (bit==-1)
            {
                done = 1;
                break;
            }
            code |= bit << i;
        }
        if (done)
            break;
        if (code==256)
        {
            lzw->code_len = init_code_len;
            lzw->dict_size = (1 << lzw->code_len)*sizeof(uint8_t *);
            lzw->dict = realloc(lzw->dict, lzw->dict_size);
        }
        // put 'prev'+'c' into the dictionary
        if (prev>=0)
        {
            lzw->dict[lzw->dict_i] = malloc(strlen(lzw->dict[prev])+2);
            uint8_t chr = lzw->dict[code==lzw->dict_i ? prev : code][0];
            sprintf(lzw->dict[lzw->dict_i], "%s%c", lzw->dict[prev], chr);
            lzw->dict_i++;
            if (lzw->dict_i==(1 << lzw->code_len))
            {
                lzw->code_len++;
                lzw->dict_size = (1 << lzw->code_len)*sizeof(uint8_t *);
                lzw->dict = realloc(lzw->dict, lzw->dict_size);
            }
        }
        if (!lzw->dict[code])
            return 1; // no entry for this code
        dst_w(ctx, lzw->dict[code], strlen(lzw->dict[code]));
        prev = code;
    }
    return 0;
}

void lzw_deinit(lzw_t *lzw)
{
    free(lzw->dict);
    memset(lzw, 0, sizeof(lzw_t));
}
