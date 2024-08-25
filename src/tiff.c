/**
 * @brief Baseline TIFF Image Encoder/Decoder
 * @source https://www.fileformat.info/format/tiff/egff.htm
 */

#include "util.h"
#include <image.h>

#include <byteswap.h>
#include <stdio.h>
#include <string.h>

image_t *image_load_tiff(const char *path) {
  FILE *f = fopen(path, "rb");
  HANDLE(f, "no such file", return NULL);

// Current Exit Procedure
#define EXIT                                                                   \
  {                                                                            \
    fclose(f);                                                                 \
    return NULL;                                                               \
  }

  // Read File Header
  uc header[8];
  HANDLE(fread(header, 8, 1, f), "failed to read header", EXIT);

  int endian = 0; // 0 -> little endian, 1 -> big endian
  if (strncasecmp((char *)header, "II", 2)) {
    if (strncasecmp((char *)header, "MM", 2))
      endian = 1;
    else {
      ERROR("invalid file");
      EXIT
    }
  }

  HANDLE(endian == 0, "big endian is unsupported", EXIT);

  u16 id_num = *(u16 *)&header[2];
  HANDLE(id_num == 42, "invalid version number", EXIT);

  u32 p = *(u32 *)&header[4];
  HANDLE(p != 0, "invalid offset", EXIT);

  // Read first IFD Entry (only one for this reader)
  fseek(f, p, SEEK_SET); // TODO check
  u16 count;
  fread(&count, 2, 1, f); // TODO check
  for (u16 i = 0; i < count; i++) {
    uc tag[12];
    fread(&tag, 12, 1, f);
    u16 id = *(u16 *)&tag[0];
    u16 type = *(u16 *)&tag[2];
    u32 count = *(u32 *)&tag[4];
    u32 offset = *(u32 *)&tag[8];

    if (id != 256 && id != 257)
      continue;

    u32 cursor = ftell(f);
    fseek(f, offset, SEEK_SET);
    u16 val;
    fread(&val, 2, 1, f);
    fseek(f, cursor, SEEK_SET);

    if (id == 256)
      printf("width %u\n", *(u16 *)&offset);
    if (id == 257)
      printf("height %u\n", *(u16 *)&offset);
  }

  //   u16 tag = *(u16 *)&entry[0];
  //   u16 type = *(u16 *)&entry[2];
  //   u32 count = *(u32 *)&entry[4];
  //   p = *(u32 *)&entry[8];

  fclose(f);

  return NULL;
}

// int image_save_qoi(image_t image, const char *path) {
//   HANDLE(image_is_valid(image), "invalid image", return 1);

//   FILE *f = fopen(path, "wb");
//   HANDLE(f, "failed to create file", return 1);

//   // Write Header
//   unsigned char header[14];
//   memset(header, 0, 14);
//   strncpy((char *)header, "qoif", 4);
//   *(uint32_t *)&header[4] = __builtin_bswap32(image.width);
//   *(uint32_t *)&header[8] = __builtin_bswap32(image.height);
//   *(uint8_t *)&header[12] = image.channels <= 3 ? 3 : 4;
//   *(uint8_t *)&header[13] = 0; // TODO

//   HANDLE(fwrite(header, 14, 1, f), "failed to write header", {
//     fclose(f);
//     return 1;
//   });

//   // Start Writing Chunks
//   uint32_t array[64] = {};
//   unsigned char prev[4] = {0, 0, 0, 255}; // previous color
//   uint32_t cursor = 0;
//   while (cursor <= image.width * image.height) {
//     unsigned char *current = &image.data[cursor * image.channels];

//     // Try encoding by repetition
//     {
//       uint8_t count = 0;
//       while (
//           !memcmp(&image.data[cursor * image.channels], prev, image.channels)
//           && count < 62) {
//         count++;
//         cursor++;
//       }

//       if (count > 0) {
//         uint8_t run = 0b11000000 | ((count - 1) & 0b00111111);
//         fwrite(&run, 1, 1, f);
//         continue;
//       }
//     }

//     // Try encoding by indexing
//     {
//       uint32_t id = HASH(current[0], current[1], current[2],
//                          image.channels == 3 ? prev[3] : current[3]);

//       if (!memcmp(&image.data[array[id] * image.channels], current,
//                   image.channels)) {
//         uint8_t index = 0b00000000 | (id & 0b00111111);
//         unsigned char *d = &image.data[array[id] * image.channels];
//         printf("%u %u %u\n", d[0], d[1], d[2]);
//         HANDLE(fwrite(&index, 1, 1, f), "failed to write index", continue);

//         memcpy(prev, current, image.channels);
//         array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
//         cursor++;
//         continue;
//       }
//     }

//     // Try encoding by difference
//     {
//       if (image.channels == 3 ||
//           (image.channels == 4 && current[3] == prev[3])) {
//         // Diff
//         int raw[3] = {current[0] - prev[0], current[1] - prev[1],
//                       current[2] - prev[2]};
//         if ((raw[0] >= -2 && raw[0] <= 1) && (raw[1] >= -2 && raw[1] <= 1) &&
//             (raw[2] >= -2 && raw[2] <= 1)) {
//           uint8_t dr = raw[0] + 2, dg = raw[1] + 2, db = raw[2] + 2;
//           uint8_t diff = 0b01000000 | ((dr & 0b00000011) << 4) |
//                          ((dg & 0b00000011) << 2) | (db & 0b00000011);
//           fwrite(&diff, 1, 1, f);

//           memcpy(prev, current, image.channels);
//           array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
//           cursor++;
//           continue;
//         }

//         // Luma
//         raw[0] = raw[0] - raw[1], raw[2] = raw[2] - raw[1];
//         if ((raw[1] >= -32 && raw[1] <= 31) && (raw[0] >= -8 && raw[0] <= 7)
//         &&
//             (raw[2] >= -8 && raw[2] <= 7)) {
//           uint8_t dr = raw[0] + 8, dg = raw[1] + 32, db = raw[2] + 8;
//           uint8_t diffg = 0b10000000 | (dg & 0b00111111);
//           uint8_t drb = ((dr & 0b00001111) << 4) | (db & 0b00001111);
//           fwrite(&diffg, 1, 1, f);
//           fwrite(&drb, 1, 1, f);

//           memcpy(prev, current, image.channels);
//           array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
//           cursor++;
//           continue;
//         }
//       }
//     }

//     // encode RGB(A)
//     {
//       uint8_t tag = 0b11111110;
//       if (image.channels == 4 && current[3] != prev[3])
//         tag = 0b11111111;
//       HANDLE(fwrite(&tag, 1, 1, f), "failed to write RGB(A) tag", continue);
//       HANDLE(fwrite(current, (tag == 0b11111110) ? 3 : 4, 1, f),
//              "failed to write RGB(A) data", continue);

//       memcpy(prev, current, image.channels);
//       array[HASH(prev[0], prev[1], prev[2], prev[3])] = cursor;
//       cursor++;
//     }
//   }

//   // Write End Sequence
//   uint8_t end = 0b00000000;
//   for (int i = 0; i < 7; i++)
//     fwrite(&end, 1, 1, f);
//   end = 0b00000001;
//   fwrite(&end, 1, 1, f);

//   fclose(f);

//   return 0;
// }