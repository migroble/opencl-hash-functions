/*
 * Copyright (c) 2024 Miguel "Peppermint" Robledo
 *
 * SPDX-License-Identifier: 0BSD
 */

#define CHUNK_BYTES 64

typedef long unsigned int u64;
typedef unsigned int u32;
typedef unsigned char u8;

typedef struct {
  u32 offset;
  u32 length;
} metadata_t;

typedef struct {
  u32 h[5];
} hash_t;

u32 left_rotate(u32 v, u32 n) { return v << n | v >> (32 - n); }

kernel void sha1(global metadata_t *metadata, global u8 *data,
                 global hash_t *hashes) {
  u32 idx = get_global_id(0);

  global metadata_t *m = metadata + idx;

  global u8 *message = data + m->offset;
  u32 len = m->length;

  // Initialize variables
  u32 h0 = 0x67452301;
  u32 h1 = 0xEFCDAB89;
  u32 h2 = 0x98BADCFE;
  u32 h3 = 0x10325476;
  u32 h4 = 0xC3D2E1F0;

  u64 ml = 8 * len;

  // Process the message in successive 512-bit chunks:
  u32 curr = 0;
  u32 n_chunks = len / CHUNK_BYTES + 1;
  for (u32 c = 0; c < n_chunks; ++c) {
    u8 chunk[CHUNK_BYTES];
    u8 *out = chunk;

    u32 next = curr + CHUNK_BYTES;
    for (; curr < next && curr < len; ++curr) {
      *out++ = message[curr];
    }

    // Pre-processing of last chunk
    if (curr < next) {
      *out = 0x80;

      for (; curr < next - 8; ++curr) {
        *++out = 0;
      }

      *out++ = ml >> 56;
      *out++ = ml >> 48;
      *out++ = ml >> 40;
      *out++ = ml >> 32;
      *out++ = ml >> 24;
      *out++ = ml >> 16;
      *out++ = ml >> 8;
      *out++ = ml >> 0;
    }

    u32 w[80];

    for (u32 i = 0; i < 16; ++i) {
      w[i] = (u32)(*(chunk + 4 * i + 0) << 24 | *(chunk + 4 * i + 1) << 16 |
                   *(chunk + 4 * i + 2) << 8 | *(chunk + 4 * i + 3));
    }

    // Message schedule: extend the sixteen 32-bit words into eitghty 32-bit
    // words:
    u32 i;
    for (i = 16; i < 32; ++i) {
      w[i] = left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    for (; i < 80; ++i) {
      w[i] = left_rotate(w[i - 6] ^ w[i - 16] ^ w[i - 28] ^ w[i - 32], 2);
    }

    // Initialize hash value for this chunk:
    u32 a = h0;
    u32 b = h1;
    u32 c = h2;
    u32 d = h3;
    u32 e = h4;
    u32 f;
    u32 k;
    u32 temp;

    // Main loop:
    for (u32 i = 0; i < 80; ++i) {
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      temp = left_rotate(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = left_rotate(b, 30);
      b = a;
      a = temp;
    }

    // Add this chunk's hash to result so far:
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  global hash_t *output = hashes + idx;
  output->h[0] = h0;
  output->h[1] = h1;
  output->h[2] = h2;
  output->h[3] = h3;
  output->h[4] = h4;
}
