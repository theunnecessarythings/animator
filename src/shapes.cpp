#include "ecs.h"

#include "shapes.h"

void RectangleShape::rebuildPath() const {
  m_path.reset();
  m_path.addRect(SkRect::MakeWH(width, height), SkPathDirection::kCW, 0);
}

void CircleShape::rebuildPath() const {
  m_path.reset();
  m_path.addCircle(0, 0, radius, SkPathDirection::kCW);
}

SkPath createRegularPolygramPath(SkPath &path, int num_vertices, float radius,
                                 int density, float start_angle) {
  if (num_vertices < 3 || radius <= 0 || density < 1)
    return path;

  float angle_increment = 2 * M_PI / num_vertices;
  float current_angle = start_angle;

  for (int i = 0; i < num_vertices; i += density) {
    float x = radius * cos(current_angle);
    float y = radius * sin(current_angle);
    if (i == 0) {
      path.moveTo(x, y);
    } else {
      path.lineTo(x, y);
    }
    current_angle += angle_increment * density;
  }
  path.close();
  return path;
}

void RegularPolygramShape::rebuildPath() const {
  m_path.reset();
  createRegularPolygramPath(m_path, num_vertices, radius, density, start_angle);
}

void LineShape::rebuildPath() const {
  m_path.reset();
  m_path.moveTo(x1, y1);
  m_path.lineTo(x2, y2);
}
