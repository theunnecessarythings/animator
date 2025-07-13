#include "camera.h"
#include "script_pch.h"
#include <cmath> // For M_PI
#include <random>

// Helper function for linear interpolation between two points
SkPoint lerp(const SkPoint &pA, const SkPoint &pB, float t) {
  return SkPoint::Make((1 - t) * pA.x() + t * pB.x(),
                       (1 - t) * pA.y() + t * pB.y());
}

// Helper function to get the control points for a sub-section of a Bezier curve
std::array<SkPoint, 4> get_bezier_sub_segment(const SkPoint &p0,
                                              const SkPoint &p1,
                                              const SkPoint &p2,
                                              const SkPoint &p3, float t) {
  auto p10 = lerp(p0, p1, t);
  auto p11 = lerp(p1, p2, t);
  auto p12 = lerp(p2, p3, t);
  auto p20 = lerp(p10, p11, t);
  auto p21 = lerp(p11, p12, t);
  auto p30 = lerp(p20, p21, t);
  return {p0, p10, p20, p30};
}

// Helper function to evaluate a cubic Bezier curve at time t
SkPoint evaluate_bezier(const SkPoint &p0, const SkPoint &p1, const SkPoint &p2,
                        const SkPoint &p3, float t) {
  float one_minus_t = 1.0f - t;
  float one_minus_t_sq = one_minus_t * one_minus_t;
  float t_sq = t * t;

  float x = one_minus_t_sq * one_minus_t * p0.x() +
            3 * one_minus_t_sq * t * p1.x() + 3 * one_minus_t * t_sq * p2.x() +
            t_sq * t * p3.x();

  float y = one_minus_t_sq * one_minus_t * p0.y() +
            3 * one_minus_t_sq * t * p1.y() + 3 * one_minus_t * t_sq * p2.y() +
            t_sq * t * p3.y();

  return SkPoint::Make(x, y);
}

// Helper function to calculate the tangent of a cubic Bezier curve at time t
SkPoint evaluate_bezier_tangent(const SkPoint &p0, const SkPoint &p1,
                                const SkPoint &p2, const SkPoint &p3, float t) {
  float one_minus_t = 1.0f - t;
  float t_sq = t * t;

  float tx = 3 * one_minus_t * one_minus_t * (p1.x() - p0.x()) +
             6 * one_minus_t * t * (p2.x() - p1.x()) +
             3 * t_sq * (p3.x() - p2.x());

  float ty = 3 * one_minus_t * one_minus_t * (p1.y() - p0.y()) +
             6 * one_minus_t * t * (p2.y() - p1.y()) +
             3 * t_sq * (p3.y() - p2.y());

  return SkPoint::Make(tx, ty);
}

struct BezierSegment {
  SkPoint p0, p1, p2, p3;
};

class PathAnimationScript : public IScript {
private:
  std::vector<BezierSegment> path_segments;
  SkPath drawn_path;
  int num_segments = 15;
  float animation_duration = 20.0f; // in seconds

  std::mt19937 rng;

  float random_float(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(rng);
  }

public:
  PathAnimationScript() {
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
  }
  ~PathAnimationScript() override = default;

  void on_start(flecs::entity entity, flecs::world &world) override {
    std::cout << "Path animation script started for entity " << entity.id()
              << std::endl;

    path_segments.clear();
    drawn_path.reset();

    // Generate the first point randomly
    SkPoint p0 = SkPoint::Make(random_float(100, 400), random_float(100, 400));
    drawn_path.moveTo(p0.x(), p0.y());

    SkPoint p1 = SkPoint::Make(p0.x() + random_float(-150, 150),
                               p0.y() + random_float(-150, 150));
    SkPoint p2 = SkPoint::Make(p0.x() + random_float(-50, 250),
                               p0.y() + random_float(-50, 250));
    SkPoint p3 = SkPoint::Make(p2.x() + random_float(-150, 150),
                               p2.y() + random_float(-150, 150));
    path_segments.push_back({p0, p1, p2, p3});

    // Generate subsequent segments to be smooth
    for (int i = 2; i <= num_segments; ++i) {
      SkPoint prev_p3 = p3;
      SkPoint prev_p2 = p2;

      p0 = prev_p3;
      // Reflected control point for C1 continuity
      p1 = SkPoint::Make(2 * p0.x() - prev_p2.x(), 2 * p0.y() - prev_p2.y());

      // New random control points
      p2 = SkPoint::Make(p0.x() + random_float(-100, 100),
                         p0.y() + random_float(-100, 100));
      p3 = SkPoint::Make(p2.x() + random_float(-150, 150),
                         p2.y() + random_float(-150, 150));
      path_segments.push_back({p0, p1, p2, p3});
    }
  }

  void on_update(flecs::entity entity, flecs::world &world, float dt,
                 float total_time) override {
    (void)world; // Suppress unused warning
    (void)dt;    // Suppress unused warning

    TransformComponent &transform = entity.get_mut<TransformComponent>();
    // Calculate the current progress through the animation loop
    float progress = fmod(total_time / animation_duration, 1.0f);

    // Determine which segment and what 't' value we are at
    int total_segments = path_segments.size();
    float current_segment_progress = progress * total_segments;
    int segment_index = static_cast<int>(floor(current_segment_progress));
    float t = current_segment_progress - segment_index;

    if (segment_index >= total_segments) {
      segment_index = total_segments - 1;
      t = 1.0f;
    }
    const BezierSegment &segment = path_segments[segment_index];

    // Calculate the position and tangent on the curve
    SkPoint pos =
        evaluate_bezier(segment.p0, segment.p1, segment.p2, segment.p3, t);
    SkPoint tan = evaluate_bezier_tangent(segment.p0, segment.p1, segment.p2,
                                          segment.p3, t);

    // Update the entity's transform
    transform.x = pos.x();
    transform.y = pos.y();
    transform.rotation = atan2(tan.y(), tan.x());

    // Center the camera on the entity
    SkPoint center = Camera::get_center();
    Camera::pan(center.x() - pos.x(), center.y() - pos.y());

    drawn_path.reset();
    if (path_segments.size() > 0) {
      drawn_path.moveTo(path_segments[0].p0.x(), path_segments[0].p0.y());
    }

    for (int i = 0; i < segment_index; ++i) {
      const BezierSegment &seg = path_segments[i];
      drawn_path.cubicTo(seg.p1.x(), seg.p1.y(), seg.p2.x(), seg.p2.y(),
                         seg.p3.x(), seg.p3.y());
    }

    auto last_segment = path_segments[segment_index];
    if (t > 0) {
      // Add the current segment up to the current 't'
      auto sub_segment =
          get_bezier_sub_segment(last_segment.p0, last_segment.p1,
                                 last_segment.p2, last_segment.p3, t);
      drawn_path.cubicTo(sub_segment[1].x(), sub_segment[1].y(),
                         sub_segment[2].x(), sub_segment[2].y(),
                         sub_segment[3].x(), sub_segment[3].y());
    }
  }

  void on_draw(flecs::entity entity, flecs::world &world,
               SkCanvas *canvas) override {
    (void)world; // Suppress unused warning
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(255, 255, 255, 255)); // Opaque white
    paint.setStyle(SkPaint::kStroke_Style); // Stroke style for paths

    TransformComponent &transform = entity.get_mut<TransformComponent>();
    canvas->save();
    // Undo the entity's transformation
    canvas->rotate(-transform.rotation * 180 / M_PI);
    canvas->translate(-transform.x, -transform.y);

    if (!drawn_path.isEmpty()) {
      // Draw the glow layer
      paint.setColor(
          SkColorSetARGB(128, 255, 255, 255)); // Semi-transparent white
      paint.setStrokeWidth(8);                 // Wider stroke for the glow
      canvas->drawPath(drawn_path, paint);

      // Draw the main path on top
      paint.setColor(SK_ColorWHITE);
      paint.setStrokeWidth(4);
      paint.setMaskFilter(nullptr); // Remove mask filter for the main path
      canvas->drawPath(drawn_path, paint);
    }

    canvas->restore();

    // Draw the highlight circle at the entity's current position
    paint.setStyle(SkPaint::kStroke_Style); // Stroke style for annulus
    paint.setStrokeWidth(3);                // Annulus thickness
    paint.setColor(SK_ColorWHITE);
    canvas->drawCircle(0, 0, 8, paint);
  }
};

// Implement the C-style factory functions that the engine will call.
extern "C" {
IScript *create_script() { return new PathAnimationScript(); }

void destroy_script(IScript *script) { delete script; }
}
