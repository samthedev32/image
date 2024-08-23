#include <image.h>

int main() {
  image_t *im = image_load_qoi("../../../examples/test.qoi");
  image_save_qoi(*im, "new.qoi");
  image_free(im);

  im = image_load_qoi("new.qoi");
  image_save_bmp(*im, "new.bmp");
  // image_free(im);
  return 0;
}