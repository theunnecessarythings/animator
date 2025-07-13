#include "camera.h"
#include "canvas.h"

SkiaCanvasWidget* Camera::s_canvas = nullptr;

void Camera::setCanvas(SkiaCanvasWidget* canvas) {
    s_canvas = canvas;
}

void Camera::pan(float dx, float dy) {
    if (s_canvas)
        s_canvas->pan(dx, dy);
}

void Camera::zoom(float factor, float x, float y) {
    if (s_canvas)
        s_canvas->zoom(factor, QPointF(x, y));
}

void Camera::reset() {
    if (s_canvas)
        s_canvas->resetView();
}

SkPoint Camera::get_center() {
    if (s_canvas) {
        QPointF center = s_canvas->getViewCenter();
        return SkPoint::Make(center.x(), center.y());
    }
    return SkPoint::Make(0.0f, 0.0f);
}
