#ifndef RENDERER_H
#define RENDERER_H

#include "mobject.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"

// Draws a Mobject using its internal SkPaint.
inline void draw_mobject(SkCanvas* canvas, const Mobject& m) {
    canvas->drawPath(m.path, m.paint);
}

#endif // RENDERER_H
