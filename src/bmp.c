/**
 * @brief BMP Loading & Saving
 */

#include "util.h"
#include <image.h>

#include <string.h>

image_t *image_load_bmp(const char *path) {
  FILE *f = fopen(path, "rb");
  HANDLE(f, "no such file", return NULL);

  // Read File Header
  struct {
    char signature[2];
    uint32_t size;
    uint32_t data;
  } header;

  unsigned char header_raw[14];
  HANDLE(fread(&header_raw, 14, 1, f), "failed to read header", {
    fclose(f);
    return NULL;
  });

  memcpy(header.signature, header_raw, 2);
  memcpy(&header.size, &header_raw[0x02], 4);
  memcpy(&header.data, &header_raw[0x0A], 4);

  HANDLE(!strncasecmp(header.signature, "BM", 2), "invalid signature", {
    fclose(f);
    return NULL;
  });

  // Read DIB Header
  uint32_t dibsize = 0;
  HANDLE(fread(&dibsize, 4, 1, f), "failed to read DIB Header Size", {
    fclose(f);
    return NULL;
  });

  HANDLE(dibsize == 40, "unsupported BID Header", {
    fclose(f);
    return NULL;
  });

  struct {
    uint32_t width, height;
    uint16_t planes;
    uint16_t bpp; // bits per pixel
    uint32_t compression;
    uint32_t size;
    uint32_t XpixelsPerM, YpixelsPerM;
    uint32_t colorsUsed;
    uint32_t importantColors;
  } info;

  HANDLE(fread(&info, 36, 1, f), "failed to read BID Header", {
    fclose(f);
    return NULL;
  });

  // Check Static Values
  HANDLE(info.width != 0 && info.height != 0 && info.planes == 1,
         "invalid image size", {
           fclose(f);
           return NULL;
         });

  HANDLE(info.compression == 0, "compression is not supported", {
    fclose(f);
    return NULL;
  });

  info.size = info.width * info.height;

  // Create Image
  image_t *out = image_allocate(info.width, info.height, 3);

  HANDLE(out, "failed to create image", {
    fclose(f);
    return NULL;
  });

  // Read Image Data
  if (info.bpp == 4 || info.bpp == 8) {
    // Read Palette
    HANDLE(info.colorsUsed == 0, "invalid palette size", {
      image_free(out);
      fclose(f);
      return NULL;
    });

    unsigned char palette[info.colorsUsed * 4];
    int read = fread(palette, sizeof(*palette), info.colorsUsed, f);
    HANDLE(read != info.colorsUsed, "failed to read palette", {
      image_free(out);
      fclose(f);
      return NULL;
    });

    if (ftell(f) != header.data) {
      WARNING("cursor does not match data offset");
      fseek(f, header.data, SEEK_SET);
    }

    // Read Image Data
    unsigned char rawID[info.size];
    HANDLE(fread(rawID, info.size, 1, f), "failed to read image data", {
      image_free(out);
      fclose(f);
      return NULL;
    });

    for (int i = 0; i < info.size; i++)
      memcpy(&out->data[i * 3], &palette[rawID[i]], 3);

    fclose(f);
  } else {
    // Read Image Data
    if (ftell(f) != header.data) {
      WARNING("cursor does not match data offset");
      printf("%li\n", ftell(f));
      printf("%i\n", header.data);

      fseek(f, header.data, SEEK_SET);
    }

    HANDLE(info.bpp == 24, "BPP not yet supported", {
      image_free(out);
      fclose(f);
      return NULL;
    });

    HANDLE(fread(out->data, info.size * 3, 1, f), "failed to read image data", {
      image_free(out);
      fclose(f);
      return NULL;
    });

    fclose(f);
  }

  return out;
}

// TODO works with 3 channels; otherwise breaks
int image_save_bmp(image_t image, const char *path) {
  HANDLE(image_is_valid(image), "invalid image", return 1);

  FILE *f = fopen(path, "wb");
  HANDLE(f, "failed to create file", return 1);

  unsigned char header[14];
  memset(header, 0, 14);
  header[0] = 'B', header[1] = 'M';
  *(uint32_t *)&header[2] = 0; // write 0, will change later
  *(uint32_t *)&header[6] = 0;
  *(uint32_t *)&header[10] = 0; // write 0, will change later

  HANDLE(fwrite(header, 14, 1, f), "failed to write header", {
    fclose(f);
    return 1;
  });

  unsigned char info[40];
  memset(info, 0, 40);
  *(uint32_t *)&info[0] = 40;
  *(uint32_t *)&info[4] = image.width;
  *(uint32_t *)&info[8] = image.height;
  *(uint32_t *)&info[12] = 1;                            // 1 plane
  *(uint32_t *)&info[14] = image.channels >= 3 ? 24 : 1; // bpp
  *(uint32_t *)&info[16] = 0;                            // no compression
  *(uint32_t *)&info[20] =
      image.width * image.height; // image size (can be set to 0)
  *(uint32_t *)&info[24] = 0;     // xppm
  *(uint32_t *)&info[28] = 0;     // yppm
  *(uint32_t *)&info[32] = 0;     // colors used
  *(uint32_t *)&info[36] = 0;     // important colors

  HANDLE(fwrite(info, 40, 1, f), "failed to write info header", {
    fclose(f);
    return 1;
  });

  uint32_t dataStart = ftell(f); // start of image data

  for (int i = 0; i < image.width * image.height; i++) {
    unsigned char *current =
        &image.data[image.width * image.height * image.channels -
                    i * image.channels];
    unsigned char value[3] = {current[2] * current[3] / 255,
                              current[1] * current[3] / 255,
                              current[0] * current[3] / 255};
    fwrite(&value, 3, 1, f);
  }

  uint32_t fileSize = ftell(f);

  fseek(f, 2, SEEK_SET);
  fwrite(&fileSize, 4, 1, f);
  fseek(f, 10, SEEK_SET);
  fwrite(&dataStart, 4, 1, f);

  fclose(f);

  return 0;
}