#include "config.h"
#include "common.h"
#include "bitstream.h"

void bitstream_init_r(bitstream_t *bs, read_func_t r, void *ctx)
{
    bs->cb = r;
    bs->ctx = ctx;
    bs->buf = 0;
    bs->mask = 256;
}

void bitstream_init_w(bitstream_t *bs, write_func_t w, void *ctx)
{
    bs->cb = w;
    bs->ctx = ctx;
    bs->buf = 0;
    bs->buf_pos = 0;
}

void bitstream_write(bitstream_t *bs, int code, uint8_t code_len)
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

int bitstream_read(bitstream_t *bs)
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
