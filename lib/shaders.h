#ifndef SHADERS_H
#define SHADERS_H

#include "include/core/SkColor.h"
#include "include/core/SkPoint.h"
#include "include/core/SkShader.h"
#include "include/effects/SkGradientShader.h"
#include <vector>

// --- Gradient Shaders --- //
inline sk_sp<SkShader> linear_gradient(const SkPoint pts[2],
                                       const SkColor colors[], int color_count,
                                       SkTileMode mode = SkTileMode::kClamp) {
  return SkGradientShader::MakeLinear(pts, colors, nullptr, color_count, mode);
}

inline sk_sp<SkShader> radial_gradient(const SkPoint &center, float radius,
                                       const SkColor colors[], int color_count,
                                       SkTileMode mode = SkTileMode::kClamp) {
  return SkGradientShader::MakeRadial(center, radius, colors, nullptr,
                                      color_count, mode);
}

// --- Image Shaders (Placeholder for now) --- //
// Requires SkImage, which might be loaded from file or generated.
// inline sk_sp<SkShader> image_shader(sk_sp<SkImage> image, SkTileMode tile_x,
// SkTileMode tile_y, const SkMatrix* local_matrix = nullptr) {
//     return image->makeShader(tile_x, tile_y, local_matrix);
// }

#endif // SHADERS_H
