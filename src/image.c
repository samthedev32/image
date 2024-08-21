/**
 * @brief Basic Image Management
 */

#include <image.h>

#include <malloc.h>

int image_is_valid(image_t image) {
  return image.width != 0 && image.height != 0 && image.channels != 0 &&
         image.data != NULL;
}

image_t *image_allocate(uint32_t width, uint32_t height, uint32_t channels) {
  image_t *out = malloc(sizeof(image_t));

  if (!out) {
    // TODO error
    return NULL;
  }

  out->width = width, out->height = height;
  out->channels = channels;
  out->data = malloc(out->width * out->height * out->channels);

  if (!out->data) {
    // TODO error
    free(out);
    return NULL;
  }

  return out;
}

void image_free(image_t *image) {
  if (!image)
    return;

  free(image->data);
  free(image);
}