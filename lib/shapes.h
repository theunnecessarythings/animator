#ifndef SHAPES_H
#define SHAPES_H

#include "include/core/SkPoint.h"
#include "mobject.h"
#include <vector>

// Creates a circle.
inline Mobject create_circle(const SkPoint &center, float radius) {
  Mobject m;
  m.path.addCircle(center.x(), center.y(), radius);
  return m;
}

// Creates a square.
inline Mobject create_square(const SkPoint &center, float size) {
  SkRect rect = SkRect::MakeXYWH(center.x() - size / 2, center.y() - size / 2,
                                 size, size);
  Mobject m;
  m.path.addRect(rect);
  return m;
}

// Creates a line.
inline Mobject create_line(const SkPoint &start, const SkPoint &end) {
  Mobject m;
  m.path.moveTo(start);
  m.path.lineTo(end);
  return m;
}

// Creates a regular polygon.
inline Mobject create_regular_polygon(const SkPoint &center, int num_sides,
                                      float radius) {
  Mobject m;
  if (num_sides < 3)
    return m;

  float angle_step = 2.0f * M_PI / num_sides;
  SkPoint first_point = {center.x() + radius, center.y()};
  m.path.moveTo(first_point);

  for (int i = 1; i < num_sides; ++i) {
    float angle = i * angle_step;
    SkPoint p = {center.x() + radius * cosf(angle), center.y() + sinf(angle)};
    m.path.lineTo(p);
  }
  m.path.close();
  return m;
}

// Creates an arrow.
inline Mobject create_arrow(const SkPoint &start, const SkPoint &end,
                            float tip_length = 15.0f, float tip_angle = 0.5f) {
  Mobject m;
  m.path.moveTo(start);
  m.path.lineTo(end);

  SkVector vec = end - start;
  vec.normalize();

  SkPoint tip_base = end;
  SkVector back_vec = {-vec.x(), -vec.y()};

  SkMatrix rot_pos, rot_neg;
  rot_pos.setRotate(tip_angle * 180 / M_PI);
  rot_neg.setRotate(-tip_angle * 180 / M_PI);

  // SkVector tip_side1 = back_vec;
  SkSpan<SkVector> tip_side1(&back_vec, 1);
  rot_pos.mapVectors(tip_side1);
  tip_side1[0].setLength(tip_length);

  // SkVector tip_side2 = back_vec;
  SkSpan<SkVector> tip_side2(&back_vec, 1);
  rot_neg.mapVectors(tip_side2);
  tip_side2[0].setLength(tip_length);

  m.path.moveTo(tip_base + tip_side1[0]);
  m.path.lineTo(tip_base);
  m.path.lineTo(tip_base + tip_side2[0]);

  return m;
}

#endif // SHAPES_H
