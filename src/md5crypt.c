/* md5crypt - implementacao compacta de MD5 + "$1$" crypt (dominio publico).
 *
 * Usada pelo chiconws para gravar a senha do root em /etc/passwd no formato
 * $1$salt$hash (o mesmo que o dropbear/uClibc entendem).
 *
 * Compilar para teste no host:
 *   gcc -O2 -o md5crypt md5crypt.c -DMD5CRYPT_MAIN
 *   ./md5crypt senha salt
 *   openssl passwd -1 -salt salt senha    # deve dar o mesmo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    unsigned char buffer[64];
} MD5_CTX;

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define STEP(f, a, b, c, d, x, t, s) \
    (a) += f((b), (c), (d)) + (x) + (t); \
    (a) = ROTL((a), (s)); \
    (a) += (b);

static void md5_transform(uint32_t state[4], const unsigned char block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    int i;
    for (i = 0; i < 16; i++) {
        x[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);
    }
    STEP(F, a, b, c, d, x[0], 0xd76aa478, 7)
    STEP(F, d, a, b, c, x[1], 0xe8c7b756, 12)
    STEP(F, c, d, a, b, x[2], 0x242070db, 17)
    STEP(F, b, c, d, a, x[3], 0xc1bdceee, 22)
    STEP(F, a, b, c, d, x[4], 0xf57c0faf, 7)
    STEP(F, d, a, b, c, x[5], 0x4787c62a, 12)
    STEP(F, c, d, a, b, x[6], 0xa8304613, 17)
    STEP(F, b, c, d, a, x[7], 0xfd469501, 22)
    STEP(F, a, b, c, d, x[8], 0x698098d8, 7)
    STEP(F, d, a, b, c, x[9], 0x8b44f7af, 12)
    STEP(F, c, d, a, b, x[10], 0xffff5bb1, 17)
    STEP(F, b, c, d, a, x[11], 0x895cd7be, 22)
    STEP(F, a, b, c, d, x[12], 0x6b901122, 7)
    STEP(F, d, a, b, c, x[13], 0xfd987193, 12)
    STEP(F, c, d, a, b, x[14], 0xa679438e, 17)
    STEP(F, b, c, d, a, x[15], 0x49b40821, 22)

    STEP(G, a, b, c, d, x[1], 0xf61e2562, 5)
    STEP(G, d, a, b, c, x[6], 0xc040b340, 9)
    STEP(G, c, d, a, b, x[11], 0x265e5a51, 14)
    STEP(G, b, c, d, a, x[0], 0xe9b6c7aa, 20)
    STEP(G, a, b, c, d, x[5], 0xd62f105d, 5)
    STEP(G, d, a, b, c, x[10], 0x02441453, 9)
    STEP(G, c, d, a, b, x[15], 0xd8a1e681, 14)
    STEP(G, b, c, d, a, x[4], 0xe7d3fbc8, 20)
    STEP(G, a, b, c, d, x[9], 0x21e1cde6, 5)
    STEP(G, d, a, b, c, x[14], 0xc33707d6, 9)
    STEP(G, c, d, a, b, x[3], 0xf4d50d87, 14)
    STEP(G, b, c, d, a, x[8], 0x455a14ed, 20)
    STEP(G, a, b, c, d, x[13], 0xa9e3e905, 5)
    STEP(G, d, a, b, c, x[2], 0xfcefa3f8, 9)
    STEP(G, c, d, a, b, x[7], 0x676f02d9, 14)
    STEP(G, b, c, d, a, x[12], 0x8d2a4c8a, 20)

    STEP(H, a, b, c, d, x[5], 0xfffa3942, 4)
    STEP(H, d, a, b, c, x[8], 0x8771f681, 11)
    STEP(H, c, d, a, b, x[11], 0x6d9d6122, 16)
    STEP(H, b, c, d, a, x[14], 0xfde5380c, 23)
    STEP(H, a, b, c, d, x[1], 0xa4beea44, 4)
    STEP(H, d, a, b, c, x[4], 0x4bdecfa9, 11)
    STEP(H, c, d, a, b, x[7], 0xf6bb4b60, 16)
    STEP(H, b, c, d, a, x[10], 0xbebfbc70, 23)
    STEP(H, a, b, c, d, x[13], 0x289b7ec6, 4)
    STEP(H, d, a, b, c, x[0], 0xeaa127fa, 11)
    STEP(H, c, d, a, b, x[3], 0xd4ef3085, 16)
    STEP(H, b, c, d, a, x[6], 0x04881d05, 23)
    STEP(H, a, b, c, d, x[9], 0xd9d4d039, 4)
    STEP(H, d, a, b, c, x[12], 0xe6db99e5, 11)
    STEP(H, c, d, a, b, x[15], 0x1fa27cf8, 16)
    STEP(H, b, c, d, a, x[2], 0xc4ac5665, 23)

    STEP(I, a, b, c, d, x[0], 0xf4292244, 6)
    STEP(I, d, a, b, c, x[7], 0x432aff97, 10)
    STEP(I, c, d, a, b, x[14], 0xab9423a7, 15)
    STEP(I, b, c, d, a, x[5], 0xfc93a039, 21)
    STEP(I, a, b, c, d, x[12], 0x655b59c3, 6)
    STEP(I, d, a, b, c, x[3], 0x8f0ccc92, 10)
    STEP(I, c, d, a, b, x[10], 0xffeff47d, 15)
    STEP(I, b, c, d, a, x[1], 0x85845dd1, 21)
    STEP(I, a, b, c, d, x[8], 0x6fa87e4f, 6)
    STEP(I, d, a, b, c, x[15], 0xfe2ce6e0, 10)
    STEP(I, c, d, a, b, x[6], 0xa3014314, 15)
    STEP(I, b, c, d, a, x[13], 0x4e0811a1, 21)
    STEP(I, a, b, c, d, x[4], 0xf7537e82, 6)
    STEP(I, d, a, b, c, x[11], 0xbd3af235, 10)
    STEP(I, c, d, a, b, x[2], 0x2ad7d2bb, 15)
    STEP(I, b, c, d, a, x[9], 0xeb86d391, 21)

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5_init(MD5_CTX *ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

static void md5_update(MD5_CTX *ctx, const unsigned char *in, unsigned int len) {
    unsigned int i, idx, part;
    idx = (ctx->count[0] >> 3) & 0x3f;
    if ((ctx->count[0] += len << 3) < (len << 3)) ctx->count[1]++;
    ctx->count[1] += len >> 29;
    part = 64 - idx;
    if (len >= part) {
        memcpy(&ctx->buffer[idx], in, part);
        md5_transform(ctx->state, ctx->buffer);
        for (i = part; i + 63 < len; i += 64) md5_transform(ctx->state, &in[i]);
        idx = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[idx], &in[i], len - i);
}

static void md5_final(unsigned char digest[16], MD5_CTX *ctx) {
    static unsigned char padding[64] = {0x80};
    unsigned char bits[8];
    unsigned int idx, pad;
    for (idx = 0; idx < 8; idx++) bits[idx] = (ctx->count[idx >> 2] >> ((idx % 4) * 8)) & 0xff;
    idx = (ctx->count[0] >> 3) & 0x3f;
    pad = (idx < 56) ? (56 - idx) : (120 - idx);
    md5_update(ctx, padding, pad);
    md5_update(ctx, bits, 8);
    for (idx = 0; idx < 4; idx++) {
        digest[idx * 4] = ctx->state[idx] & 0xff;
        digest[idx * 4 + 1] = (ctx->state[idx] >> 8) & 0xff;
        digest[idx * 4 + 2] = (ctx->state[idx] >> 16) & 0xff;
        digest[idx * 4 + 3] = (ctx->state[idx] >> 24) & 0xff;
    }
}

static const char itoa64[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void to64(char *s, unsigned long v, int n) {
    while (--n >= 0) {
        *s++ = itoa64[v & 0x3f];
        v >>= 6;
    }
}

/* Gera "$1$<salt>$<hash>" em out (>= 35 bytes). Salt max 8 chars. */
int md5_crypt(const char *pw, const char *salt, char *out) {
    MD5_CTX ctx, ctx1;
    unsigned char final[16];
    int i, sl, pl;
    const char *sp, *ep;
    char saltstr[9];

    sp = salt;
    if (sp[0] == '$' && sp[1] == '1' && sp[2] == '$') sp += 3;
    for (ep = sp; *ep && *ep != '$' && ep < sp + 8; ep++) ;
    sl = ep - sp;
    if (sl > 8) sl = 8;
    memcpy(saltstr, sp, sl);
    saltstr[sl] = 0;
    pl = strlen(pw);

    md5_init(&ctx);
    md5_update(&ctx, (const unsigned char *)pw, pl);
    md5_update(&ctx, (const unsigned char *)"$1$", 3);
    md5_update(&ctx, (const unsigned char *)saltstr, sl);

    md5_init(&ctx1);
    md5_update(&ctx1, (const unsigned char *)pw, pl);
    md5_update(&ctx1, (const unsigned char *)saltstr, sl);
    md5_update(&ctx1, (const unsigned char *)pw, pl);
    md5_final(final, &ctx1);
    for (i = pl; i > 0; i -= 16) {
        md5_update(&ctx, final, (i > 16) ? 16 : i);
    }
    for (i = pl; i > 0; i >>= 1) {
        if (i & 1) md5_update(&ctx, (const unsigned char *)"\0", 1);
        else md5_update(&ctx, (const unsigned char *)pw, 1);
    }
    md5_final(final, &ctx);

    for (i = 0; i < 1000; i++) {
        md5_init(&ctx1);
        if (i & 1) md5_update(&ctx1, (const unsigned char *)pw, pl);
        else md5_update(&ctx1, final, 16);
        if (i % 3) md5_update(&ctx1, (const unsigned char *)saltstr, sl);
        if (i % 7) md5_update(&ctx1, (const unsigned char *)pw, pl);
        if (i & 1) md5_update(&ctx1, final, 16);
        else md5_update(&ctx1, (const unsigned char *)pw, pl);
        md5_final(final, &ctx1);
    }

    {
        char *p = out;
        int n;
        n = snprintf(p, 4, "$1$");
        p += n;
        memcpy(p, saltstr, sl);
        p += sl;
        *p++ = '$';
        to64(p, ((unsigned long)final[0] << 16) | ((unsigned long)final[6] << 8) | final[12], 4); p += 4;
        to64(p, ((unsigned long)final[1] << 16) | ((unsigned long)final[7] << 8) | final[13], 4); p += 4;
        to64(p, ((unsigned long)final[2] << 16) | ((unsigned long)final[8] << 8) | final[14], 4); p += 4;
        to64(p, ((unsigned long)final[3] << 16) | ((unsigned long)final[9] << 8) | final[15], 4); p += 4;
        to64(p, ((unsigned long)final[4] << 16) | ((unsigned long)final[10] << 8) | final[5], 4); p += 4;
        to64(p, (unsigned long)final[11], 2); p += 2;
        *p = 0;
    }
    return 0;
}

#ifdef MD5CRYPT_MAIN
int main(int argc, char **argv) {
    char out[64];
    if (argc < 3) {
        fprintf(stderr, "uso: %s <senha> <salt>\n", argv[0]);
        return 1;
    }
    md5_crypt(argv[1], argv[2], out);
    puts(out);
    return 0;
}
#endif
