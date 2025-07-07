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

void ArcShape::rebuildPath() const {
  m_path.reset();
  m_path.addArc(SkRect::MakeXYWH(arc_center_x - radius, arc_center_y - radius,
                                 2 * radius, 2 * radius),
                start_angle, angle);
}

void ArcBetweenPointsShape::rebuildPath() const {
  m_path.reset();

  SkPoint p1 = SkPoint::Make(x1, y1);
  SkPoint p2 = SkPoint::Make(x2, y2);

  float dist = SkPoint::Distance(p1, p2);

  float actual_radius = radius;
  float actual_angle = angle; // This is the desired sweep angle in degrees

  if (actual_radius == 0.0f) { // Auto-calculate radius based on angle
    if (actual_angle == 0.0f) {
      m_path.moveTo(x1, y1);
      m_path.lineTo(x2, y2);
      return;
    }
    // Convert angle to radians for trigonometric functions
    float half_angle_rad = (actual_angle / 2.0f) * M_PI / 180.0f;
    actual_radius = (dist / 2.0f) / sin(half_angle_rad);
  } else { // Radius is provided, calculate angle
    float halfdist = dist / 2.0f;
    if (actual_radius < halfdist) {
      // Radius is too small to connect the points. Draw a line instead.
      m_path.moveTo(x1, y1);
      m_path.lineTo(x2, y2);
      return;
    }
    // Calculate the angle based on the provided radius
    float cos_half_angle = (actual_radius - sqrt(actual_radius * actual_radius -
                                                 halfdist * halfdist)) /
                           actual_radius;
    actual_angle = acos(cos_half_angle) * 2.0f * 180.0f / M_PI;
  }

  // Calculate center of the arc
  SkPoint mid_point = SkPoint::Make((x1 + x2) / 2.0f, (y1 + y2) / 2.0f);
  float d_mid_to_center_sq =
      actual_radius * actual_radius - (dist * dist / 4.0f);
  float d_mid_to_center = sqrt(d_mid_to_center_sq);

  SkVector vec_p1_to_p2 = p2 - p1;
  SkVector perp_vec =
      SkVector::Make(-vec_p1_to_p2.fY, vec_p1_to_p2.fX); // Perpendicular vector
  perp_vec.normalize();

  // There are two possible centers. We need to choose one based on the sign of
  // the angle.
  SkPoint center1 = mid_point + perp_vec * d_mid_to_center;
  SkPoint center2 = mid_point - perp_vec * d_mid_to_center;

  SkPoint arc_center;
  float start_angle_degrees;

  // Determine which center to use
  float cross_product1 = (p1.fX - center1.fX) * (p2.fY - center1.fY) -
                         (p1.fY - center1.fY) * (p2.fX - center1.fX);

  if ((actual_angle > 0 && cross_product1 > 0) ||
      (actual_angle < 0 && cross_product1 < 0)) {
    arc_center = center1;
  } else {
    arc_center = center2;
  }

  // Calculate start angle in degrees (Skia's convention: 0 at 3 o'clock,
  // positive clockwise)
  start_angle_degrees =
      atan2(p1.fY - arc_center.fY, p1.fX - arc_center.fX) * 180.0f / M_PI;

  // Adjust start angle to be within [0, 360)
  start_angle_degrees = fmod(start_angle_degrees, 360.0f);
  if (start_angle_degrees < 0) {
    start_angle_degrees += 360.0f;
  }

  // Skia's addArc draws clockwise for positive sweep.
  // If our actual_angle is positive (counter-clockwise), we need a negative
  // sweep in Skia. If our actual_angle is negative (clockwise), we need a
  // positive sweep in Skia.
  float skia_sweep_angle =
      -actual_angle; // Invert for Skia's clockwise positive convention

  m_path.addArc(SkRect::MakeXYWH(arc_center.fX - actual_radius,
                                 arc_center.fY - actual_radius,
                                 2 * actual_radius, 2 * actual_radius),
                start_angle_degrees, skia_sweep_angle);
}
