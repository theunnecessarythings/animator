#ifndef ANIMATION_H
#define ANIMATION_H

#include "mobject.h"
#include <functional>

#include "include/core/SkColor.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkPoint.h"
#include "include/core/SkShader.h"

#include <algorithm>
#include <vector>

// Helper function to resample a path to a given number of points.
inline SkPath resample_path(const SkPath &path, int num_points) {
  SkPath resampled_path;
  SkPathMeasure measure(path, false);
  float length = measure.getLength();
  float interval = length / (num_points - 1);

  SkPoint p;
  SkVector v;

  for (int i = 0; i < num_points; ++i) {
    measure.getPosTan(i * interval, &p, &v);
    if (i == 0) {
      resampled_path.moveTo(p);
    } else {
      resampled_path.lineTo(p);
    }
  }
  return resampled_path;
}

// An Animation is a function that transforms a Mobject over time (t from 0 to
// 1).
typedef std::function<Mobject(const Mobject &, float)> Animation;

// Applies a series of animations sequentially.
inline Animation chain(const std::vector<Animation> &animations) {
  return [=](const Mobject &m, float t) {
    if (animations.empty())
      return m;

    Mobject current_mobject = m; // Start with the original mobject

    float segment_duration = 1.0f / animations.size();

    for (size_t i = 0; i < animations.size(); ++i) {
      float segment_start_t = i * segment_duration;
      float segment_end_t = (i + 1) * segment_duration;

      if (t >= segment_end_t) {
        // This animation segment is completed, apply it fully
        current_mobject = animations[i](current_mobject, 1.0f);
      } else if (t >= segment_start_t) {
        // This is the current active animation segment, apply partially
        float local_t = (t - segment_start_t) / segment_duration;
        current_mobject = animations[i](current_mobject, local_t);
        break; // We found the active segment, no need to process further
      } else {
        // This animation segment is yet to start, and subsequent ones too.
        // The mobject remains as it was after previous completed animations.
        break;
      }
    }
    return current_mobject;
  };
}

// --- Basic Animations --- //

inline Animation fade_in() {
  return [](const Mobject &m, float t) {
    Mobject new_m = m;
    new_m.paint.setAlpha(static_cast<U8CPU>(t * 255));
    return new_m;
  };
}

inline Animation move_to(const SkPoint &new_pos) {
  return [=](const Mobject &m, float t) {
    SkRect bounds = m.path.getBounds();
    SkPoint current_pos = bounds.center();
    SkPoint interpolated_pos =
        SkPoint::Make(current_pos.x() + (new_pos.x() - current_pos.x()) * t,
                      current_pos.y() + (new_pos.y() - current_pos.y()) * t);
    Mobject new_m = m;
    new_m.path.offset(interpolated_pos.x() - current_pos.x(),
                      interpolated_pos.y() - current_pos.y());
    return new_m;
  };
}

inline Animation scale(float factor) {
  return [=](const Mobject &m, float t) {
    Mobject new_m = m;
    SkMatrix matrix;
    float current_scale = 1.0f + (factor - 1.0f) * t;
    matrix.setScale(current_scale, current_scale, m.path.getBounds().centerX(),
                    m.path.getBounds().centerY());
    new_m.path.transform(matrix);
    return new_m;
  };
}

inline Animation rotate(float degrees) {
  return [=](const Mobject &m, float t) {
    Mobject new_m = m;
    SkMatrix matrix;
    matrix.setRotate(degrees * t, m.path.getBounds().centerX(),
                     m.path.getBounds().centerY());
    new_m.path.transform(matrix);
    return new_m;
  };
}

// --- Path Animations --- //

// Morphs one path into another, interpolating paint properties.
inline Animation transform(const Mobject &target_m) {
  return [=](const Mobject &start_m, float t) {
    Mobject new_m = start_m; // Start with the initial mobject's properties

    // 1. Interpolate Path (Resampling)
    const int NUM_RESAMPLE_POINTS =
        100; // Arbitrary number of points for resampling
    SkPath start_resampled = resample_path(start_m.path, NUM_RESAMPLE_POINTS);
    SkPath target_resampled = resample_path(target_m.path, NUM_RESAMPLE_POINTS);

    SkPath interpolated_path;
    SkPathMeasure measure_start(start_resampled, false);
    SkPathMeasure measure_target(target_resampled, false);

    SkPoint p_start, p_target, p_interpolated;
    SkVector v_dummy;

    for (int i = 0; i < NUM_RESAMPLE_POINTS; ++i) {
      float dist_t = static_cast<float>(i) / (NUM_RESAMPLE_POINTS - 1);
      measure_start.getPosTan(dist_t * measure_start.getLength(), &p_start,
                              &v_dummy);
      measure_target.getPosTan(dist_t * measure_target.getLength(), &p_target,
                               &v_dummy);

      p_interpolated.fX = p_start.fX + (p_target.fX - p_start.fX) * t;
      p_interpolated.fY = p_start.fY + (p_target.fY - p_start.fY) * t;

      if (i == 0) {
        interpolated_path.moveTo(p_interpolated);
      } else {
        interpolated_path.lineTo(p_interpolated);
      }
    }
    new_m.path = interpolated_path;

    // 2. Interpolate Paint Properties
    // Color interpolation
    SkColor start_color = start_m.paint.getColor();
    SkColor target_color = target_m.paint.getColor();

    auto clamp_color_component = [](float val) {
      return static_cast<U8CPU>(std::max(0.0f, std::min(255.0f, val)));
    };

    SkColor interpolated_color = SkColorSetARGB(
        clamp_color_component(
            SkColorGetA(start_color) +
            (SkColorGetA(target_color) - SkColorGetA(start_color)) * t),
        clamp_color_component(
            SkColorGetR(start_color) +
            (SkColorGetR(target_color) - SkColorGetR(start_color)) * t),
        clamp_color_component(
            SkColorGetG(start_color) +
            (SkColorGetG(target_color) - SkColorGetG(start_color)) * t),
        clamp_color_component(
            SkColorGetB(start_color) +
            (SkColorGetB(target_color) - SkColorGetB(start_color)) * t));
    new_m.paint.setColor(interpolated_color);

    // Stroke width interpolation
    float interpolated_stroke_width =
        start_m.paint.getStrokeWidth() +
        (target_m.paint.getStrokeWidth() - start_m.paint.getStrokeWidth()) * t;
    new_m.paint.setStrokeWidth(interpolated_stroke_width);

    // Style (fill/stroke) interpolation
    // This is a simplified approach. A more robust solution would involve
    // interpolating alpha for both fill and stroke components.
    if (t < 0.5) {
      new_m.paint.setStyle(start_m.paint.getStyle());
    } else {
      new_m.paint.setStyle(target_m.paint.getStyle());
    }

    // Mask Filters and Image Filters (simple cross-fade for now)
    if (t < 0.5) {
      new_m.paint.setMaskFilter(start_m.paint.refMaskFilter());
      new_m.paint.setImageFilter(start_m.paint.refImageFilter());
    } else {
      new_m.paint.setMaskFilter(target_m.paint.refMaskFilter());
      new_m.paint.setImageFilter(target_m.paint.refImageFilter());
    }

    return new_m;
  };
}

// Animates the drawing of a path.
inline Animation show_creation() {
  return [](const Mobject &m, float t) {
    Mobject new_m = m;
    SkPathMeasure measure(m.path, false);
    SkPath new_path;
    measure.getSegment(0, measure.getLength() * t, &new_path, true);
    new_m.path = new_path;
    return new_m;
  };
}

#endif // ANIMATION_H
