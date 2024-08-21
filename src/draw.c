#include <image.h>

void image_draw_fill(image_t image, const unsigned char *color,
                     uint8_t channels) {
  if (!image_is_valid(image))
    return;

  for (int i = 0; i < image.width * image.height; i++) {
    for (int j = 0; j < image.channels; j++) {
      image.data[i * image.channels + j] = j < channels ? color[j] : 0;
    }
  }
}