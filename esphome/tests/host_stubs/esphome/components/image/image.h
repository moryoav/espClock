#pragma once
#include <cstdint>
#include "esphome/core/color.h"
namespace esphome::image {
enum ImageType { IMAGE_TYPE_RGB565 };
class Image {
 public:
  Image(const uint8_t *data, int width, int height, int bpp = 16)
      : data_(data), width_(width), height_(height), bpp_(bpp) {}
  int get_width() const { return width_; }
  int get_height() const { return height_; }
  int get_bpp() const { return bpp_; }
  ImageType get_type() const { return IMAGE_TYPE_RGB565; }
  bool has_transparency() const { return true; }
  const uint8_t *get_data_start() const { return data_; }
  Color get_pixel(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return Color(0, 0, 0, 0);
    const unsigned index = unsigned(y * width_ + x);
    if (bpp_ == 32) {
      const uint8_t *pixel = data_ + index * 4;
      return Color(pixel[0], pixel[1], pixel[2], pixel[3]);
    }
    uint16_t value;
    uint8_t alpha;
    if (bpp_ == 16) {
      value = data_[index * 2] | (uint16_t(data_[index * 2 + 1]) << 8);
      alpha = data_[width_ * height_ * 2 + index];
    } else {
      value = (uint16_t(data_[index * 3]) << 8) | data_[index * 3 + 1];
      alpha = data_[index * 3 + 2];
    }
    uint8_t r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
    return Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), alpha);
  }
 private:
  const uint8_t *data_;
  int width_, height_, bpp_;
};
}
