#pragma once
#define FLECS_CPP
#include <flecs.h>

#include "include/codec/SkCodec.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathEffect.h"
#include "include/effects/SkCornerPathEffect.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkDiscretePathEffect.h"

#include <QJsonObject>
#include <QMetaObject>
#include <QMetaProperty>

#include <sol/sol.hpp>
#include <string>
#include <type_traits>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Basic aliases & helpers
// ─────────────────────────────────────────────────────────────────────────────
using Entity = flecs::entity;
inline Entity kInvalidEntity = {};

template <typename Gadget>
static void fillFromJson(Gadget &dst, const QJsonObject &src) {
  const QMetaObject &mo = Gadget::staticMetaObject;
  for (int i = mo.propertyOffset(); i < mo.propertyCount(); ++i) {
    QMetaProperty p = mo.property(i);
    if (src.contains(p.name()))
      p.writeOnGadget(&dst, src[p.name()].toVariant());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Component definitions
// ─────────────────────────────────────────────────────────────────────────────
struct NameComponent {
  std::string name;
};

struct MaterialComponent {
  SkColor color = SK_ColorBLUE;
  bool isFilled = true;
  bool isStroked = true;
  float strokeWidth = 1.f;
  bool antiAliased = true;
};

struct AnimationComponent {
  float entryTime = 0.f, exitTime = 5.f;
};

struct ScriptComponent {
  std::string scriptPath;
  std::string startFunction = "on_start";
  std::string updateFunction = "on_update";
  std::string destroyFunction = "on_destroy";
  sol::table scriptEnv; // each script gets its own Lua env
};

struct PathEffectComponent {
  enum class Type { None, Dash, Corner, Discrete };
  Type type = Type::None;

  // for Dash
  std::vector<float> dashIntervals = {10.f, 5.f};
  float dashPhase = 0.f;

  // for Corner
  float cornerRadius = 5.f;

  // for Discrete
  float discreteLength = 10.f;
  float discreteDeviation = 5.f;

  sk_sp<SkPathEffect> makePathEffect() const {
    switch (type) {
    case Type::Dash:
      if (dashIntervals.size() >= 2 && (dashIntervals.size() % 2 == 0)) {
        return SkDashPathEffect::Make(
            SkSpan<const SkScalar>(dashIntervals.data(), dashIntervals.size()),
            dashPhase);
      }
      break;
    case Type::Corner:
      return SkCornerPathEffect::Make(cornerRadius);
    case Type::Discrete:
      return SkDiscretePathEffect::Make(discreteLength, discreteDeviation, 0);
    case Type::None:
    default:
      return nullptr;
    }
    return nullptr;
  }
};

struct SceneBackgroundComponent {}; // tag

struct TransformComponent {
  float x = 0.f, y = 0.f;
  float rotation = 0.f;     // radians
  float sx = 1.f, sy = 1.f; // scale
};

class Shape;
struct ShapeComponent {
  std::unique_ptr<Shape> shape;
  ShapeComponent() = default;
  ShapeComponent(std::unique_ptr<Shape> s) : shape(std::move(s)) {}
  ShapeComponent(ShapeComponent &&) noexcept = default;
  ShapeComponent &operator=(ShapeComponent &&) noexcept = default;
};
