#include "config.h"
#include "common.h"
#include "lzw.h"
#include "bitstream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define LZW_BUFFER_SIZE 256
#define LZW_INIT_CODE_LEN 9

void lzw_init(lzw_t *lzw)
{
    uint8_t code_len = LZW_INIT_CODE_LEN;
    lzw->dict_size = (1 << code_len)*sizeof(uint8_t *);
    lzw->dict = malloc(lzw->dict_size*sizeof(lzw_dict_entry_t));
    static uint8_t dict_def_data[257];
    static int dict_def_data_init = 0;
    if (!dict_def_data_init)
    {
        // first 256 entries are initialized with their indices
        for (int i = 0; i<256; i++)
        {
            lzw->dict[i].size = 1;
            lzw->dict[i].data = dict_def_data+i;
            dict_def_data[i] = i;
        }
        // 'reset' code
        lzw->dict[256].size = 0;
        lzw->dict[256].data = dict_def_data+256;
        dict_def_data[256] = 0;
        dict_def_data_init = 1;
    }
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
    size_t req_size = 1 << lzw->code_len;
    if (req_size > lzw->dict_size)
    {
        lzw->dict_size = req_size;
        lzw->dict = realloc(lzw->dict, lzw->dict_size);
    }
    uint8_t buf[LZW_BUFFER_SIZE*2];
    uint8_t *inp = buf;
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
        for (int i = lzw->dict_i-1; i>=0; i--)
        {
            if (lzw->dict[i].size &&
                !memcmp(lzw->dict[i].data, inp, lzw->dict[i].size))
            {
                outc = i;
                break;
            }
        }
        assert(outc>=0);
        bitstream_write(&bs_w, outc, lzw->code_len);
        inp += lzw->dict[outc].size;
        // expand the dictionary if necessary
        if (lzw->dict_i==(1 << lzw->code_len))
        {
            lzw->code_len++;
            lzw->dict_size = 1 << lzw->code_len;
            lzw->dict = realloc(lzw->dict,
                lzw->dict_size*sizeof(lzw_dict_entry_t));
        }
        // add this match, along with the next character, to the dictionary
        lzw_dict_entry_t *di = &lzw->dict[lzw->dict_i];
        di->size = lzw->dict[outc].size+1;
        di->data = malloc(di->size);
        memcpy(di->data, lzw->dict[outc].data, lzw->dict[outc].size);
        di->data[di->size-1] = *inp;
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
    size_t req_size = 1 << lzw->code_len;
    if (req_size > lzw->dict_size)
    {
        lzw->dict_size = req_size;
        lzw->dict = realloc(lzw->dict,
            lzw->dict_size*sizeof(lzw_dict_entry_t));
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
            lzw->dict_size = 1 << lzw->code_len;
            lzw->dict = realloc(lzw->dict,
                lzw->dict_size*sizeof(lzw_dict_entry_t));
        }
        // put 'prev'+'c' into the dictionary
        if (prev>=0)
        {
            lzw_dict_entry_t *di = &lzw->dict[lzw->dict_i];
            di->size = lzw->dict[prev].size+1;
            di->data = malloc(di->size);
            uint8_t chr = lzw->dict[code==lzw->dict_i ? prev : code].data[0];
            memcpy(di->data, lzw->dict[prev].data, lzw->dict[prev].size);
            di->data[di->size-1] = chr;
            lzw->dict_i++;
            if (lzw->dict_i==(1 << lzw->code_len))
            {
                lzw->code_len++;
                lzw->dict_size = 1 << lzw->code_len;
                lzw->dict = realloc(lzw->dict,
                    lzw->dict_size*sizeof(lzw_dict_entry_t));
            }
        }
        // XXX: zeroize dictionary to make this work
        if (!lzw->dict[code].size)
            return 1; // no entry for this code
        dst_w(ctx, lzw->dict[code].data, lzw->dict[code].size);
        prev = code;
    }
    return 0;
}

void lzw_deinit(lzw_t *lzw)
{
    free(lzw->dict);
    // XXX: free dict entry data
    memset(lzw, 0, sizeof(lzw_t));
}
