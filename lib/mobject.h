#ifndef MOBJECT_H
#define MOBJECT_H

#include "include/core/SkPoint.h"
#include "include/core/SkPath.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkShader.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkMaskFilter.h"
#include "include/effects/SkImageFilters.h"
#include <vector>

// Represents a generic mathematical object (Mobject).
// This is a plain data structure holding the state of a visual element.
struct Mobject {
    SkPath path;
    SkColor color = SK_ColorWHITE; // Kept for backward compatibility/simplicity if needed
    float stroke_width = 4.0f;     // Kept for backward compatibility/simplicity if needed
    SkPaint paint;                 // The primary paint object for rendering

    Mobject() {
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(stroke_width);
        paint.setColor(color);
    }

    // Chaining methods for styling
    Mobject set_color(SkColor c) const {
        Mobject new_m = *this;
        new_m.color = c;
        new_m.paint.setColor(c);
        return new_m;
    }

    Mobject set_fill_color(SkColor c) const {
        Mobject new_m = *this;
        new_m.paint.setColor(c);
        new_m.paint.setStyle(SkPaint::kFill_Style);
        return new_m;
    }

    Mobject set_stroke_color(SkColor c) const {
        Mobject new_m = *this;
        new_m.paint.setColor(c);
        new_m.paint.setStyle(SkPaint::kStroke_Style);
        return new_m;
    }

    Mobject set_stroke_width(float width) const {
        Mobject new_m = *this;
        new_m.stroke_width = width;
        new_m.paint.setStrokeWidth(width);
        return new_m;
    }

    Mobject set_shader(sk_sp<SkShader> shader) const {
        Mobject new_m = *this;
        new_m.paint.setShader(std::move(shader));
        return new_m;
    }

    Mobject set_mask_filter(sk_sp<SkMaskFilter> filter) const {
        Mobject new_m = *this;
        new_m.paint.setMaskFilter(std::move(filter));
        return new_m;
    }

    Mobject set_image_filter(sk_sp<SkImageFilter> filter) const {
        Mobject new_m = *this;
        new_m.paint.setImageFilter(std::move(filter));
        return new_m;
    }

    Mobject set_blend_mode(SkBlendMode mode) const {
        Mobject new_m = *this;
        new_m.paint.setBlendMode(mode);
        return new_m;
    }

    // You can add more setters for other SkPaint properties as needed
};

#endif // MOBJECT_H
