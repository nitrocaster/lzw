#include "config.h"
#include "common.h"
#include "lzw.h"
#include <stdio.h>
#include <string.h>

static int open_files(char *src_name, char *dst_name,
    FILE **src, FILE **dst)
{
    *src = fopen(src_name, "rb");
    if (!*src)
    {
        puts("can't open source file.");
        return 1;
    }
    *dst = fopen(dst_name, "wb");
    if (!*dst)
    {
        fclose(*src);
        puts("can't open destination file.");
        return 1;
    }
    return 0;
}

typedef struct
{
    FILE *src;
    FILE *dst;
} context_t;

static size_t read_src(void *ctx, uint8_t *dst, size_t count)
{
    context_t *c = ctx;
    return fread(dst, 1, count, c->src);
}

static size_t write_dst(void *ctx, uint8_t *src, size_t count)
{
    context_t *c = ctx;
    return fwrite(src, 1, count, c->dst);
}

static void print_usage()
{
    const char *usage_str = "usage: lzw {compress|decompress} <src> <dst>";
    puts(usage_str);
}

int main(int argc, char *argv[])
{
    //  0   1        2   3
    // lzw compress src dst
    if (argc!=4)
    {
        print_usage();
        return 1;
    }
    char mode = 'x';
    if (!strcmp(argv[1], "compress"))
        mode = 'c';
    else if (!strcmp(argv[1], "decompress"))
        mode = 'd';
    if (mode=='x')
    {
        print_usage();
        return 1;
    }
    context_t ctx;
    if (open_files(argv[2], argv[3], &ctx.src, &ctx.dst))
        return 1;
    lzw_t lzw;
    lzw_init(&lzw);
    int ret = 0;
    switch (mode)
    {
    case 'c':
        ret = lzw_compress(&lzw, read_src, write_dst, &ctx);
        break;
    case 'd':
        ret = lzw_decompress(&lzw, read_src, write_dst, &ctx);
        break;
    }
    lzw_deinit(&lzw);
    fclose(ctx.src);
    fclose(ctx.dst);
    return ret;    
}
