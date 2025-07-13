#pragma once

#include "canvas.h" // Assuming SkiaCanvasWidget is declared here
#include "include/core/SkPoint.h"

class Camera {
public:
    static void setCanvas(SkiaCanvasWidget* canvas);
    static void pan(float dx, float dy);
    static void zoom(float factor, float x, float y);
    static void reset();
    static SkPoint get_center();

private:
    static SkiaCanvasWidget* s_canvas;
};
