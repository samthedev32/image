/**
 * @brief QOI Loading & Saving
 */

#include "util.h"
#include <image.h>

#include <string.h>

// hash rgba according to qoi specification
#define HASH(R, G, B, A) ((R * 3 + G * 5 + B * 7 + A * 11) % 64)

image_t *image_load_qoi(const char *path) {
  FILE *f = fopen(path, "rb");
  HANDLE(f, "no such file", return NULL);

  // Read File Header
  uint32_t width, height;
  uint8_t channels, colorspace;

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
  memcpy(&colorspace, &header_raw[13], 1);
  width = __builtin_bswap32(width);
  height = __builtin_bswap32(height);

  uint32_t array[64] = {};
  unsigned char prev[4] = {0, 0, 0, 255}; // previous color

  image_t *out = image_allocate(width, height, channels);
  memset(out->data, 0, width * height * channels);

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
      memcpy(prev, current, 3);
      cursor++;
    } else if (tag == 0xFF) {
      // RGBA
      HANDLE(fread(current, 4, 1, f), "failed to read rgba", continue);

      array[HASH(current[0], current[1], current[2], current[3])] = cursor;
      memcpy(prev, current, 4);
      cursor++;
    } else
      // 2 bit tags
      if (tag >> 6 == 0x00) {
        // index
        uint8_t index = tag & 0x3F;

        memcpy(current, &out->data[array[index] * channels], channels);

        memcpy(prev, current, channels);
        cursor++;
      } else if (tag >> 6 == 0x01) {
        // diff
        uint8_t dr = (tag >> 4) & 0x02, dg = (tag >> 2) & 0x02, db = tag & 0x02;

        current[0] = prev[0] + (dr - 2);
        current[1] = prev[1] + (dg - 2);
        current[2] = prev[2] + (db - 2);

        if (channels == 4)
          current[3] = prev[3];

        array[HASH(current[0], current[1], current[2], prev[3])] = cursor;
        memcpy(prev, current, 3);
        cursor++;
      } else if (tag >> 6 == 0x02) {
        // luma
        uint8_t dg = (tag & 0x3F) + 32;

        unsigned char drb;
        HANDLE(fread(&drb, 1, 1, f), "failed to read rb difference", continue);

        uint8_t drmdg = drb >> 4, dbmdg = drb & 0x0F;

        uint8_t dr = drmdg + dg + 8, db = dbmdg + dg + 8;

        current[0] = prev[0] + dr;
        current[1] = prev[1] + dg;
        current[2] = prev[2] + db;

        if (channels == 4)
          current[3] = prev[3];

        array[HASH(current[0], current[1], current[2], prev[3])] = cursor;
        memcpy(prev, current, 3);
        cursor++;
      } else if (tag >> 6 == 0x03) {
        // run
        uint8_t run = (tag & 0x3F) + 1;

        for (int i = 0; i < run; i++) {
          memcpy(current, prev, 3);
          if (channels == 4)
            current[3] = prev[3];

          cursor++;
        }
      } else if (tag == 0x01) {
        // eos
        break;
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
  header[0] = 'Q', header[1] = 'O', header[2] = 'I', header[3] = 'F';
  *(uint32_t *)&header[4] = __builtin_bswap32(image.width);
  *(uint32_t *)&header[8] = __builtin_bswap32(image.height);
  *(uint32_t *)&header[12] = image.channels <= 3 ? 3 : 4;
  *(uint32_t *)&header[13] = 0; // TODO

  HANDLE(fwrite(header, 14, 1, f), "failed to write header", {
    fclose(f);
    return 1;
  });

  // Start Writing Chunks
  uint32_t array[64] = {};
  unsigned char prev[4] = {0, 0, 0, 255}; // previous color

  fclose(f);

  return 0;
}