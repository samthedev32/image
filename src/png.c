/**
 * @brief PNG Loading & Saving
 */

#include "util.h"
#include <image.h>

#include <malloc.h>
#include <string.h>

#include <zconf.h>
#include <zlib.h>

#define IS_CRITICAL(type) ('A' <= (type)[0] && (type)[0] <= 'Z')

image_t *image_load_png(const char *path) {
  FILE *f = fopen(path, "rb");
  HANDLE(f, "no such file", return NULL);

  // Read Header
  char header[8];
  HANDLE(fread(header, 8, 1, f), "failed to read header", {
    fclose(f);
    return NULL;
  });

  HANDLE(!strncasecmp(header, "\x89PNG\x0D\x0A\x1A\x0A", 8), "invalid header", {
    fclose(f);
    return NULL;
  });

  // Read Chunks
  struct {
    uint32_t width;
    uint32_t height;
    uint8_t bitDepth;
    uint8_t colorType;
    uint8_t compressionMethod;
    uint8_t filterMethod;
    uint8_t interlanceMethod;
  } ihdr;

  struct {
    size_t size;
    unsigned char *data;
  } idat = {0, NULL};

  while (1) {
    // Chunk Data
    struct {
      uint32_t length;
      char type[4];
      unsigned char *data;
      uint32_t crc;
    } chunk;

    // Read Chunk Length & Type
    HANDLE(fread(&chunk.length, 4, 1, f) && fread(chunk.type, 4, 1, f),
           "unexpected error", {
             free(idat.data);
             fclose(f);
             return NULL;
           });

    chunk.length = __builtin_bswap32(chunk.length);

    // Read Data
    if (chunk.length > 0) {
      // Allocate Memory
      chunk.data = (unsigned char *)malloc(chunk.length);
      HANDLE(chunk.data, "failed to allocate chunk", {
        if (IS_CRITICAL(chunk.type)) {
          free(idat.data);
          fclose(f);
          return NULL;
        } else
          continue;
      })

      // Read Data
      HANDLE(fread(chunk.data, chunk.length, 1, f), "failed to read data", {
        if (IS_CRITICAL(chunk.type)) {
          free(chunk.data);
          free(idat.data);
          fclose(f);
          return NULL;
        } else {
          free(chunk.data);
          continue;
        }
      });
    }

    // Read Chunk CRC
    HANDLE(fread(&chunk.crc, 4, 1, f), "failed to read crc",
           fseek(f, 4, SEEK_CUR););
    chunk.crc = __builtin_bswap32(chunk.crc);

    if (!strncasecmp(chunk.type, "IHDR", 4)) {
      // Image Info
      ihdr.width = *(uint32_t *)&chunk.data[0];
      ihdr.height = *(uint32_t *)&chunk.data[4];
      ihdr.bitDepth = *(uint8_t *)&chunk.data[8];
      ihdr.colorType = *(uint8_t *)&chunk.data[9];
      ihdr.compressionMethod = *(uint8_t *)&chunk.data[10];
      ihdr.filterMethod = *(uint8_t *)&chunk.data[11];
      ihdr.interlanceMethod = *(uint8_t *)&chunk.data[12];

      ihdr.width = __bswap_32(ihdr.width);
      ihdr.height = __bswap_32(ihdr.height);

      int palette = ihdr.colorType & 1 << 0;
      int color = ihdr.colorType & 1 << 1;
      int alpha = ihdr.colorType & 1 << 2;

      int err = 0;

      // Validate Values
      HANDLE(ihdr.width != 0 && ihdr.height != 0, "invalid ihdr size",
             err = 1;);

      HANDLE((!color || !alpha) && ihdr.bitDepth >= 8, "invalid bit depth",
             err = 1;);

      HANDLE((!palette || !color) && ihdr.bitDepth <= 8, "invalid bit depth",
             err = 1;);

      HANDLE(ihdr.compressionMethod == 0, "unknown compression method",
             err = 1;);
      HANDLE(ihdr.filterMethod == 0, "unknown filter method", err = 1;);
      HANDLE(ihdr.interlanceMethod == 0, "unsupported interlance method",
             err = 1;);

      // exit if test(s) fail(s)
      if (err) {
        free(chunk.data);
        free(idat.data);
        fclose(f);
        return NULL;
      }

      // TODO: check chunk order
    } else if (!strncasecmp(chunk.type, "PLTE", 4)) {
      HANDLE(chunk.length % 3 == 0, "invalid palette size", {
        free(chunk.data);
        free(idat.data);
        fclose(f);
        return NULL;
      });

      uint8_t entries = chunk.length / 3;
      unsigned char palette[entries][3];

      ERROR("palette is not yet supported");
      free(chunk.data);
      free(idat.data);
      fclose(f);
      return NULL;

      //   int err = 0;

      //   if (fread(palette, 3, entries, f) != entries) {
      //     // Failed to read palette
      //     printf("Failed to read Palette\n");
      //     return out;
      //   }

      // TODO: store palette
    } else if (!strncasecmp(chunk.type, "IDAT", 4)) {
      size_t cursor = idat.size;
      idat.size += chunk.length;
      unsigned char *d = NULL;
      if (idat.data == NULL)
        d = (unsigned char *)malloc(chunk.length);
      else
        d = (unsigned char *)realloc(idat.data, idat.size);

      HANDLE(d, "failed to allocate memory", {
        free(chunk.data);
        free(idat.data);
        fclose(f);
        return NULL;
      });
      idat.data = d;

      for (int i = 0; i < chunk.length; i++)
        idat.data[cursor + i] = chunk.data[i];
    } else if (!strncasecmp(chunk.type, "IEND", 4)) {
      // TODO: finish up & return image

      // Decompress Data
      uLong dstSize =
          ihdr.width * ihdr.height * ihdr.bitDepth; // TODO: color type
      unsigned char *dst =
          (unsigned char *)malloc(sizeof(unsigned char) * dstSize);
      int result = uncompress((Bytef *)dst, &dstSize, (const Bytef *)idat.data,
                              idat.size);

      // TODO channels
      image_t *out = image_allocate(ihdr.width, ihdr.height, 3);
      HANDLE(out, "failed to create image", {});

      if (result) {
        printf("Failed to Decompress Image\n");
        return out;
      }

      // TODO filtering
      switch (ihdr.filterMethod) {
      default:
      case 0: // None
        break;

      case 1: // Sub
        break;

      case 2: // Up
        break;

      case 3: // Avarage
        break;

      case 4: // Paeth
        break;
      }

      return out;
    } else {
      free(chunk.data);

      if (IS_CRITICAL(chunk.type)) {
        free(idat.data);
        fclose(f);
        return NULL;
      }
    }
  }

  return NULL;
}