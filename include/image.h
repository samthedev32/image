#ifndef _IMAGE_H_
#define _IMAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief C/C++ Image Manipulation Library
 */

#include <stdint.h>

// enum PixelFormat {};

typedef struct {
  uint32_t width, height;
  uint8_t channels;

  unsigned char *data;
} image_t;

//////////////////////////////// Management

// Is Image Valid
int image_is_valid(image_t image);

// Allocate Image
image_t *image_allocate(uint32_t width, uint32_t height, uint32_t channels);

// Free Image
void image_free(image_t *image);

// TODO copy, resize, crop, rotate, etc

//////////////////////////////// File I/O

image_t *image_load_bmp(const char *path);
int image_save_bmp(image_t image, const char *path);

image_t *image_load_qoi(const char *path);
int image_save_qoi(image_t image, const char *path);

image_t *image_load_png(const char *path);
// TODO png, jpg, qoi

//////////////////////////////// Drawing

// Fill image with color
void image_draw_fill(image_t image, const unsigned char *color,
                     uint8_t channels);

// Draw Pixel
// void image_draw_pixel(image_t image, int x, int y, int size);

// Draw Line
// void image_draw_line(image_t image, int x, int y, int xx, int yy, int size);

// void image_draw_rect_center(image_t image, int x, int y, int w, int h,
// const unsigned char *color, uint8_t channels);

// TODO rendering, etc

#ifdef __cplusplus
}
#endif

#endif // _IMAGE_H_