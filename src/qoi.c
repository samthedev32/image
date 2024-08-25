/**
 * @brief QOI Loading & Saving
 */

#include "util.h"
#include <image.h>

#include <byteswap.h>
#include <stdio.h>
#include <string.h>

// hash rgba according to qoi specification
#define HASH(R, G, B, A) (((R) * 3 + (G) * 5 + (B) * 7 + (A) * 11) % 64)

image_t *image_load_qoi(const char *path) {
  FILE *f = fopen(path, "rb");
  HANDLE(f, "no such file", return NULL);

// Current Exit Procedure
#define EXIT                                                                   \
  {                                                                            \
    fclose(f);                                                                 \
    return NULL;                                                               \
  }

  // Read File Header
  uc header[14];
  HANDLE(fread(header, 14, 1, f), "failed to read header", EXIT);
  HANDLE(!strncasecmp((char *)header, "QOIF", 4), "invalid file", EXIT);

  u32 width = __bswap_32(*(u32 *)&header[4]),
      height = __bswap_32(*(u32 *)&header[8]);
  u8 channels = *(u8 *)&header[12];

  image_t *out = image_allocate(width, height, channels);
  memset(out->data, 0, width * height * channels);

  u32 array[64] = {};
  uc prev[4] = {0, 0, 0, 255}; // previous color
  u32 cursor = 0;
  while (cursor < width * height) {
    uc tag;
    HANDLE(fread(&tag, 1, 1, f), "failed to read tag", break);

    // ptr to current pixel
    uc *current = &out->data[cursor * channels];

    // 8 bit tags
    if (tag == 0xFE) {
      // RGB
      HANDLE(fread(current, 3, 1, f), "failed to read rgb", continue);

      if (channels == 4)
        current[3] = prev[3];

      array[HASH(current[0], current[1], current[2], prev[3])] = cursor;
      memcpy(prev, current, channels); // TODO check
      cursor++;
    } else if (tag == 0xFF) {
      // RGBA
      HANDLE(fread(current, 4, 1, f), "failed to read rgba", continue);

      memcpy(prev, current, channels); // TODO check
      array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
      cursor++;
    } else {
      // 2 bit tags
      if (tag >> 6 == 0x00) {
        // index
        u8 index = tag & 0x3F;

        memcpy(current, &out->data[array[index] * channels], channels);
        memcpy(prev, current, channels); // TODO check
        cursor++;
      } else if (tag >> 6 == 0x01) {
        // diff
        u8 dr = (tag >> 4) & 0x03, dg = (tag >> 2) & 0x03, db = tag & 0x03;
        current[0] = prev[0] + (dr - 2);
        current[1] = prev[1] + (dg - 2);
        current[2] = prev[2] + (db - 2);

        if (channels == 4)
          current[3] = prev[3];

        memcpy(prev, current, channels); // TODO check
        array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
        cursor++;
      } else if (tag >> 6 == 0x02) {
        // luma
        u8 dg = (tag & 0x3F) - 32;

        uc drb;
        HANDLE(fread(&drb, 1, 1, f), "failed to read rb difference", continue);

        u8 drmdg = drb >> 4, dbmdg = drb & 0x0F;
        u8 dr = drmdg + dg - 8, db = dbmdg + dg - 8;

        current[0] = prev[0] + dr;
        current[1] = prev[1] + dg;
        current[2] = prev[2] + db;

        if (channels == 4)
          current[3] = prev[3];

        memcpy(prev, current, channels); // TODO check
        array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
        cursor++;
      } else if (tag >> 6 == 0x03) {
        // run
        u8 run = (tag & 0b00111111) + 1;
        for (int i = 0; i < run; i++) {
          memcpy(&out->data[cursor * channels], prev, channels); // TODO check
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
  uc header[14] = {};
  strncpy((char *)header, "qoif", 4);
  *(u32 *)&header[4] = __bswap_32(image.width);
  *(u32 *)&header[8] = __bswap_32(image.height);
  *(u8 *)&header[12] = image.channels <= 3 ? 3 : 4;
  *(u8 *)&header[13] = 0; // TODO

  HANDLE(fwrite(header, 14, 1, f), "failed to write header", {
    fclose(f);
    return 1;
  });

  // Start Writing Chunks
  u32 array[64] = {};
  uc prev[4] = {0, 0, 0, 255}; // previous color
  u32 cursor = 0;
  while (cursor <= image.width * image.height) {
    uc *current = &image.data[cursor * image.channels];

    // Try encoding by repetition
    {
      u8 count = 0;
      while (
          !memcmp(&image.data[cursor * image.channels], prev, image.channels) &&
          count < 62) {
        count++;
        cursor++;
      }

      if (count > 0) {
        u8 run = 0b11000000 | ((count - 1) & 0b00111111);
        fwrite(&run, 1, 1, f);
        continue;
      }
    }

    // Try encoding by indexing
    {
      u32 id = HASH(current[0], current[1], current[2],
                    image.channels == 3 ? prev[3] : current[3]);

      if (!memcmp(&image.data[array[id] * image.channels], current,
                  image.channels)) {
        u8 index = 0b00000000 | (id & 0b00111111);
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
          u8 dr = raw[0] + 2, dg = raw[1] + 2, db = raw[2] + 2;
          u8 diff = 0b01000000 | ((dr & 0b00000011) << 4) |
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
          u8 dr = raw[0] + 8, dg = raw[1] + 32, db = raw[2] + 8;
          u8 diffg = 0b10000000 | (dg & 0b00111111);
          u8 drb = ((dr & 0b00001111) << 4) | (db & 0b00001111);
          HANDLE(fwrite(&diffg, 1, 1, f), "failed to write DIFF tag", continue);
          HANDLE(fwrite(&drb, 1, 1, f), "failed to write DIFF data", continue);

          memcpy(prev, current, image.channels);
          array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
          cursor++;
          continue;
        }
      }
    }

    // encode RGB(A)
    {
      u8 tag = 0b11111110;
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
  u8 end = 0b00000000;
  for (int i = 0; i < 7; i++)
    fwrite(&end, 1, 1, f);
  end = 0b00000001;
  fwrite(&end, 1, 1, f);

  fclose(f);

  return 0;
}