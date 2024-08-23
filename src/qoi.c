/**
 * @brief QOI Loading & Saving
 */

#include "util.h"
#include <image.h>

#include <stdio.h>
#include <string.h>

// hash rgba according to qoi specification
#define HASH(R, G, B, A) (((R) * 3 + (G) * 5 + (B) * 7 + (A) * 11) % 64)

#define IN_LIMITS(current, prev)                                               \
  ((current) >= (prev) - 2 && (current) <= (prev) + 1)

image_t *image_load_qoi(const char *path) {
  FILE *f = fopen(path, "rb");
  HANDLE(f, "no such file", return NULL);

  // Read File Header
  uint32_t width, height;
  uint8_t channels;

  unsigned char header_raw[14];
  HANDLE(fread(header_raw, 14, 1, f), "failed to read header", {
    fclose(f);
    return NULL;
  });

  HANDLE(!strncasecmp((char *)header_raw, "QOIF", 4), "invalid file", {
    fclose(f);
    return NULL;
  });

  memcpy(&width, &header_raw[4], 4);
  memcpy(&height, &header_raw[8], 4);
  memcpy(&channels, &header_raw[12], 1);

  width = __builtin_bswap32(width);
  height = __builtin_bswap32(height);

  image_t *out = image_allocate(width, height, channels);
  memset(out->data, 0, width * height * channels);

  uint32_t array[64] = {};
  unsigned char prev[4] = {0, 0, 0, 255}; // previous color
  uint32_t cursor = 0;
  while (cursor < width * height) {
    unsigned char tag;
    HANDLE(fread(&tag, 1, 1, f), "failed to read tag", break);

    // ptr to current pixel
    unsigned char *current = &out->data[cursor * channels];

    // 8 bit tags
    if (tag == 0xFE) {
      // RGB
      HANDLE(fread(current, 3, 1, f), "failed to read rgb", continue);

      if (channels == 4)
        current[3] = prev[3];

      array[HASH(current[0], current[1], current[2], prev[3])] = cursor;
      memcpy(prev, current, channels);
      cursor++;
    } else if (tag == 0xFF) {
      // RGBA
      HANDLE(fread(current, 4, 1, f), "failed to read rgba", continue);

      memcpy(prev, current, channels);
      array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
      cursor++;
    } else {
      // 2 bit tags
      if (tag >> 6 == 0x00) {
        // index
        uint8_t index = tag & 0x3F;

        memcpy(current, &out->data[array[index] * channels], channels);
        memcpy(prev, current, channels);
        cursor++;
      } else if (tag >> 6 == 0x01) {
        // diff
        uint8_t dr = (tag >> 4) & 0x03, dg = (tag >> 2) & 0x03, db = tag & 0x03;
        current[0] = prev[0] + (dr - 2);
        current[1] = prev[1] + (dg - 2);
        current[2] = prev[2] + (db - 2);

        if (channels == 4)
          current[3] = prev[3];

        memcpy(prev, current, channels);
        array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
        cursor++;
      } else if (tag >> 6 == 0x02) {
        // luma
        uint8_t dg = (tag & 0x3F) - 32;

        unsigned char drb;
        HANDLE(fread(&drb, 1, 1, f), "failed to read rb difference", continue);

        uint8_t drmdg = drb >> 4, dbmdg = drb & 0x0F;
        uint8_t dr = drmdg + dg - 8, db = dbmdg + dg - 8;

        current[0] = prev[0] + dr;
        current[1] = prev[1] + dg;
        current[2] = prev[2] + db;

        if (channels == 4)
          current[3] = prev[3];

        memcpy(prev, current, channels);
        array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
        cursor++;
      } else if (tag >> 6 == 0x03) {
        // run
        uint8_t run = (tag & 0b00111111) + 1;
        for (int i = 0; i < run; i++) {
          memcpy(&out->data[cursor * channels], prev, channels);
          cursor++;
        }
      }
      // TODO eos
    }
  }

  fclose(f);

  return out;
}

int image_save_qoi(image_t image, const char *path) {
  HANDLE(image_is_valid(image), "invalid image", return 1);

  FILE *f = fopen(path, "wb");
  HANDLE(f, "failed to create file", return 1);

  // Write Header
  unsigned char header[14];
  memset(header, 0, 14);
  strncpy((char *)header, "qoif", 4);
  *(uint32_t *)&header[4] = __builtin_bswap32(image.width);
  *(uint32_t *)&header[8] = __builtin_bswap32(image.height);
  *(uint8_t *)&header[12] = image.channels <= 3 ? 3 : 4;
  *(uint8_t *)&header[13] = 0; // TODO

  HANDLE(fwrite(header, 14, 1, f), "failed to write header", {
    fclose(f);
    return 1;
  });

  // Start Writing Chunks
  uint32_t array[64] = {};
  unsigned char prev[4] = {0, 0, 0, 255}; // previous color
  uint32_t cursor = 0;
  while (cursor <= image.width * image.height) {
    unsigned char *current = &image.data[cursor * image.channels];

    // Try encoding by repetition
    {
      uint8_t count = 0;
      while (
          !memcmp(&image.data[cursor * image.channels], prev, image.channels) &&
          count < 62) {
        count++;
        cursor++;
      }

      if (count > 0) {
        uint8_t run = 0b11000000 | ((count - 1) & 0b00111111);
        fwrite(&run, 1, 1, f);
        continue;
      }
    }

    // Try encoding by indexing
    {
      uint32_t id = HASH(current[0], current[1], current[2],
                         image.channels == 3 ? prev[3] : current[3]);

      if (!memcmp(&image.data[array[id] * image.channels], current,
                  image.channels)) {
        uint8_t index = 0b00000000 | (id & 0b00111111);
        unsigned char *d = &image.data[array[id] * image.channels];
        printf("%u %u %u\n", d[0], d[1], d[2]);
        HANDLE(fwrite(&index, 1, 1, f), "failed to write index", continue);

        memcpy(prev, current, image.channels);
        array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
        cursor++;
        continue;
      }
    }

    // Try encoding by difference
    {
      if (image.channels == 3 ||
          (image.channels == 4 && current[3] == prev[3])) {
        // Diff
        int raw[3] = {current[0] - prev[0], current[1] - prev[1],
                      current[2] - prev[2]};
        if ((raw[0] >= -2 && raw[0] <= 1) && (raw[1] >= -2 && raw[1] <= 1) &&
            (raw[2] >= -2 && raw[2] <= 1)) {
          uint8_t dr = raw[0] + 2, dg = raw[1] + 2, db = raw[2] + 2;
          uint8_t diff = 0b01000000 | ((dr & 0b00000011) << 4) |
                         ((dg & 0b00000011) << 2) | (db & 0b00000011);
          fwrite(&diff, 1, 1, f);

          memcpy(prev, current, image.channels);
          array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
          cursor++;
          continue;
        }

        // Luma
        raw[0] = raw[0] - raw[1], raw[2] = raw[2] - raw[1];
        if ((raw[1] >= -32 && raw[1] <= 31) && (raw[0] >= -8 && raw[0] <= 7) &&
            (raw[2] >= -8 && raw[2] <= 7)) {
          uint8_t dr = raw[0] + 8, dg = raw[1] + 32, db = raw[2] + 8;
          uint8_t diffg = 0b10000000 | (dg & 0b00111111);
          uint8_t drb = ((dr & 0b00001111) << 4) | (db & 0b00001111);
          fwrite(&diffg, 1, 1, f);
          fwrite(&drb, 1, 1, f);

          memcpy(prev, current, image.channels);
          array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
          cursor++;
          continue;
        }
      }
    }

    // encode RGB(A)
    {
      uint8_t tag = 0b11111110;
      if (image.channels == 4 && current[3] != prev[3])
        tag = 0b11111111;
      HANDLE(fwrite(&tag, 1, 1, f), "failed to write RGB(A) tag", continue);
      HANDLE(fwrite(current, (tag == 0b11111110) ? 3 : 4, 1, f),
             "failed to write RGB(A) data", continue);

      memcpy(prev, current, image.channels);
      array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
      cursor++;
    }
  }

  // Write End Sequence
  uint8_t end = 0b00000000;
  for (int i = 0; i < 7; i++)
    fwrite(&end, 1, 1, f);
  end = 0b00000001;
  fwrite(&end, 1, 1, f);

  fclose(f);

  return 0;
}