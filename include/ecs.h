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

#include <QJsonObject>
#include <QMetaObject>
#include <QMetaProperty>

#include <sol/sol.hpp>
#include <string>
#include <type_traits>

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
  bool isStroked = false;
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

struct SceneBackgroundComponent {}; // tag

struct TransformComponent {
  float x = 0.f, y = 0.f;
  float rotation = 0.f;     // radians
  float sx = 1.f, sy = 1.f; // scale
};

#include "shapes.h"

struct ShapeComponent {
  std::unique_ptr<Shape> shape;
  ShapeComponent() = default;
  ShapeComponent(std::unique_ptr<Shape> s) : shape(std::move(s)) {}
  ShapeComponent(ShapeComponent &&) noexcept = default;
  ShapeComponent &operator=(ShapeComponent &&) noexcept = default;
  ShapeComponent(const ShapeComponent &other) {
    if (other.shape)
      shape = other.shape->clone();
  }
  ShapeComponent &operator=(const ShapeComponent &other) {
    if (this != &other)
      shape = other.shape ? other.shape->clone() : nullptr;
    return *this;
  }
};
