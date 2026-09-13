// Frozen pre-optimization bilinear rasterizer for visual regression comparisons.
#pragma once
namespace car_clock {
void referenceDrawCarSprite(float x, float y, float angle, float scale, const Rgb &tint, uint8_t alpha = 255,
                   const esphome::image::Image *sprite = nullptr,
                   float widthScale = 1.0f, float lengthScale = 1.0f) {
  if (sprite == nullptr) sprite = gCarSprite;
  if (sprite == nullptr) return;

  // The old embedded car image was landscape, so the renderer subtracted 90 degrees.
  // assets/car.png is portrait (28x52). Keeping the old correction swapped the
  // visible long/short axes and made the seven-segment positions look scrambled.
  // Detect the source orientation from the compiled ESPHome image instead.
  const bool sourceIsPortrait = sprite->get_height() >= sprite->get_width();
  const float sourceAxisOffset = sourceIsPortrait ? 0.0f : -kHalfPi;
  const float rotation = angle + sourceAxisOffset;
  const float c = cosf(rotation), s = sinf(rotation);
  const float sourceW = float(sprite->get_width());
  const float sourceH = float(sprite->get_height());
  const float scaleX = scale * widthScale, scaleY = scale * lengthScale;
  const float halfW = (fabsf(c) * sourceW * scaleX + fabsf(s) * sourceH * scaleY) * 0.5f + 1.5f;
  const float halfH = (fabsf(s) * sourceW * scaleX + fabsf(c) * sourceH * scaleY) * 0.5f + 1.5f;

  const int16_t xMin = int16_t(std::max(0.0f, floorf(x - halfW)));
  const int16_t xMax = int16_t(std::min(float(Hardware::kLogicalWidth - 1), ceilf(x + halfW)));
  const int16_t yMin = int16_t(std::max(0.0f, floorf(y - halfH)));
  const int16_t yMax = int16_t(std::min(float(Hardware::kLogicalHeight - 1), ceilf(y + halfH)));

  for (int16_t py = yMin; py <= yMax; ++py) {
    for (int16_t px = xMin; px <= xMax; ++px) {
      const float dx = float(px) + 0.5f - x;
      const float dy = float(py) + 0.5f - y;
      const float localX = (c * dx + s * dy) / scaleX + (sourceW - 1.0f) * 0.5f;
      const float localY = (-s * dx + c * dy) / scaleY + (sourceH - 1.0f) * 0.5f;
      if (localX < 0.0f || localY < 0.0f || localX > sourceW - 1.0f || localY > sourceH - 1.0f) continue;

      const int x0 = int(floorf(localX));
      const int y0 = int(floorf(localY));
      const int x1 = std::min(x0 + 1, int(sprite->get_width()) - 1);
      const int y1 = std::min(y0 + 1, int(sprite->get_height()) - 1);
      const float fx = localX - float(x0);
      const float fy = localY - float(y0);
      const float w00 = (1.0f - fx) * (1.0f - fy);
      const float w10 = fx * (1.0f - fy);
      const float w01 = (1.0f - fx) * fy;
      const float w11 = fx * fy;

      const auto c00 = sprite->get_pixel(x0, y0);
      const auto c10 = sprite->get_pixel(x1, y0);
      const auto c01 = sprite->get_pixel(x0, y1);
      const auto c11 = sprite->get_pixel(x1, y1);

      float sr = c00.r * w00 + c10.r * w10 + c01.r * w01 + c11.r * w11;
      float sg = c00.g * w00 + c10.g * w10 + c01.g * w01 + c11.g * w11;
      float sb = c00.b * w00 + c10.b * w10 + c01.b * w01 + c11.b * w11;
      float sa = c00.w * w00 + c10.w * w10 + c01.w * w01 + c11.w * w11;

      const uint8_t pixelAlpha = static_cast<uint8_t>(clampFloat(sa * float(alpha) / 255.0f, 0.0f, 255.0f));
      if (pixelAlpha < 3) continue;
      Rgb color{
          static_cast<uint8_t>(clampFloat(sr * float(tint.r) / 255.0f, 0.0f, 255.0f)),
          static_cast<uint8_t>(clampFloat(sg * float(tint.g) / 255.0f, 0.0f, 255.0f)),
          static_cast<uint8_t>(clampFloat(sb * float(tint.b) / 255.0f, 0.0f, 255.0f)),
      };
      putPixel(px, py, color, pixelAlpha);
    }
  }
}

}
