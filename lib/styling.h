#ifndef STYLING_H
#define STYLING_H

#include "include/core/SkPaint.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkMaskFilter.h"
#include "include/effects/SkImageFilters.h"

// --- Basic Paint Creation --- //

inline SkPaint create_paint() {
    SkPaint paint;
    paint.setAntiAlias(true);
    return paint;
}

// --- Color and Style --- //

inline SkPaint fill_paint(SkColor color) {
    SkPaint paint = create_paint();
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    return paint;
}

inline SkPaint stroke_paint(SkColor color, float width = 4.0f) {
    SkPaint paint = create_paint();
    paint.setColor(color);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    return paint;
}

// --- Effects --- //

inline SkPaint blur_paint(float sigma, SkPaint base_paint = create_paint()) {
    base_paint.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle, sigma));
    return base_paint;
}

inline SkPaint drop_shadow_paint(float dx, float dy, float sigmaX, float sigmaY, SkColor color, SkPaint base_paint = create_paint()) {
    base_paint.setImageFilter(SkImageFilters::DropShadow(dx, dy, sigmaX, sigmaY, color, nullptr));
    return base_paint;
}

inline SkPaint color_filter_paint(sk_sp<SkColorFilter> filter, SkPaint base_paint = create_paint()) {
    base_paint.setColorFilter(filter);
    return base_paint;
}

inline SkPaint blend_mode_paint(SkBlendMode mode, SkPaint base_paint = create_paint()) {
    base_paint.setBlendMode(mode);
    return base_paint;
}

#endif // STYLING_H
