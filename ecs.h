#pragma once
/*
 * A **header‑only, zero‑dependency ECS** tailored for our Qt + Skia editor.
 * - Entity  = uint32_t handle    (0 == invalid)
 * - Component storage is a sparse unordered_map per type
 * - Registry owns the storage and offers `emplace<T>()`, `get<T>()`,
 * `has<T>()`.
 * - Systems are plain structs with a `tick(Registry&, float dt)` or `render()`.
 *
 * Enough to support:        ─► drag‑and‑drop creates entity + ShapeComponent +
 * Transfo m ─► RenderSystem iterates and draws via Skia ─► SceneModel (QA
 * stractItemModel) reflects Registry for the UI
 */

#include "include/codec/SkCodec.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include <QMetaType>
#include <QObject>
#include <QVariant>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// -----------------------------------------------------------------------------
//  Core types
// -----------------------------------------------------------------------------
using Entity = uint32_t;
static constexpr Entity kInvalidEntity = 0;

// ----------------------------------------------------------------------------
//  Component storage interface (type‑erased)
// ----------------------------------------------------------------------------
class IComponentStorage {
public:
  virtual ~IComponentStorage() = default;
  virtual void remove(Entity e) = 0;
};

template <class T> class ComponentStorage final : public IComponentStorage {
public:
  template <class... Args> T &emplace(Entity e, Args &&...args) {
    auto [it, ok] = data_.try_emplace(e, std::forward<Args>(args)...);
    return it->second; // returns existing if already present
  }
  bool has(Entity e) const { return data_.count(e) != 0; }
  T *get(Entity e) {
    auto it = data_.find(e);
    return it == data_.end() ? nullptr : &it->second;
  }
  const T *get(Entity e) const {
    return const_cast<ComponentStorage *>(this)->get(e);
  }

  void remove(Entity e) override { data_.erase(e); }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }

private:
  std::unordered_map<Entity, T> data_;
};

// ----------------------------------------------------------------------------
//  Registry: central API the editor interacts with
// ----------------------------------------------------------------------------
class Registry {
public:
  Entity create() { return next_++; }
  void destroy(Entity e) {
    for (auto &[_, stor] : pools_)
      stor->remove(e);
  }

  template <class T> ComponentStorage<T> &storage() {
    const std::type_index idx = typeid(T);
    auto it = pools_.find(idx);
    if (it == pools_.end()) {
      auto ptr = std::make_unique<ComponentStorage<T>>();
      auto raw = ptr.get();
      pools_.emplace(idx, std::move(ptr));
      return *raw;
    }
    return *static_cast<ComponentStorage<T> *>(it->second.get());
  }

  template <class T, class... Args> T &emplace(Entity e, Args &&...args) {
    return storage<T>().emplace(e, std::forward<Args>(args)...);
  }

  template <class T> T *get(Entity e) { return storage<T>().get(e); }

  template <class T> bool has(Entity e) const {
    auto it = pools_.find(std::type_index(typeid(T)));
    return it != pools_.end() &&
           static_cast<ComponentStorage<T> *>(it->second.get())->has(e);
  }

  // Simple view: iterate over all (Entity, T&) pairs.  Extend as needed.
  template <class T, class F> void each(F &&fn) {
    for (auto &[e, comp] : storage<T>())
      fn(e, comp);
  }

private:
  Entity next_ = 1;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>>
      pools_;
};

// -----------------------------------------------------------------------------
//  Example components – keep it trivial for now
// -----------------------------------------------------------------------------
struct TransformComponent {
  float x = 0.f, y = 0.f;
  float rotation = 0.f;     // radians
  float sx = 1.f, sy = 1.f; // scale
};

// Shape-specific properties
struct RectangleProperties {
  Q_GADGET
  Q_PROPERTY(float width MEMBER width)
  Q_PROPERTY(float height MEMBER height)
  Q_PROPERTY(float grid_xstep MEMBER grid_xstep)
  Q_PROPERTY(float grid_ystep MEMBER grid_ystep)
public:
  float width = 100.0f;
  float height = 60.0f;
  float grid_xstep = 10.0f;
  float grid_ystep = 10.0f;
};
Q_DECLARE_METATYPE(RectangleProperties);

struct CircleProperties {
  Q_GADGET
  Q_PROPERTY(float radius MEMBER radius)
public:
  float radius = 50.0f;
};
Q_DECLARE_METATYPE(CircleProperties);

struct LineProperties {
  Q_GADGET
  Q_PROPERTY(float x1 MEMBER x1)
  Q_PROPERTY(float y1 MEMBER y1)
  Q_PROPERTY(float x2 MEMBER x2)
  Q_PROPERTY(float y2 MEMBER y2)
public:
  float x1 = 0.0f, y1 = 0.0f;
  float x2 = 100.0f, y2 = 0.0f;
};
Q_DECLARE_METATYPE(LineProperties);

struct BezierProperties {
  Q_GADGET
  Q_PROPERTY(float x1 MEMBER x1)
  Q_PROPERTY(float y1 MEMBER y1)
  Q_PROPERTY(float cx1 MEMBER cx1)
  Q_PROPERTY(float cy1 MEMBER cy1)
  Q_PROPERTY(float cx2 MEMBER cx2)
  Q_PROPERTY(float cy2 MEMBER cy2)
  Q_PROPERTY(float x2 MEMBER x2)
  Q_PROPERTY(float y2 MEMBER y2)
public:
  float x1 = 0.0f, y1 = 0.0f;
  float cx1 = 25.0f, cy1 = 75.0f;
  float cx2 = 75.0f, cy2 = 25.0f;
  float x2 = 100.0f, y2 = 100.0f;
};
Q_DECLARE_METATYPE(BezierProperties);

struct TextProperties {
  Q_GADGET
  Q_PROPERTY(QString text MEMBER text)
  Q_PROPERTY(float fontSize MEMBER fontSize)
  Q_PROPERTY(QString fontFamily MEMBER fontFamily)
public:
  QString text = "Hello World";
  float fontSize = 24.0f;
  QString fontFamily = "Arial";
};
Q_DECLARE_METATYPE(TextProperties);

struct ImageProperties {
  Q_GADGET
  Q_PROPERTY(QString filePath MEMBER filePath)
public:
  QString filePath;
};

struct ShapeComponent {
  enum class Kind { Rectangle, Circle, Line, Bezier, Text, Image };
  Kind kind;
  std::variant<std::monostate, RectangleProperties, CircleProperties,
               LineProperties, BezierProperties, TextProperties,
               ImageProperties>
      properties; // Add other shape properties here as they are defined

  static const char *toString(Kind k) {
    switch (k) {
    case Kind::Rectangle:
      return "Rectangle";
    case Kind::Circle:
      return "Circle";
    case Kind::Line:
      return "Line";
    case Kind::Bezier:
      return "Bezier";
    case Kind::Text:
      return "Text";
    case Kind::Image:
      return "Image";
    }
    return "";
  }
};

struct NameComponent {
  std::string name;
};

struct MaterialComponent {
  SkColor color = SK_ColorBLACK;
  bool isFilled = true;
  bool isStroked = false;
  float strokeWidth = 1.0f;
  bool antiAliased = true;
};

struct AnimationComponent {
  float entryTime = 0.0f;
  float exitTime = 5.0f;
};
// -----------------------------------------------------------------------------
//  RenderSystem – draws entities with {Transform, Shape} via Skia
// -----------------------------------------------------------------------------

class RenderSystem {
public:
  explicit RenderSystem(Registry &r) : reg_(r) {}

  void render(SkCanvas *canvas, float currentTime) {
    reg_.each<TransformComponent>([&](Entity ent, TransformComponent &tr) {
      auto *shape = reg_.get<ShapeComponent>(ent);
      if (!shape)
        return;

      auto *material = reg_.get<MaterialComponent>(ent);
      if (!material)
        return;

      auto *animation = reg_.get<AnimationComponent>(ent);
      if (animation && (currentTime < animation->entryTime ||
                        currentTime > animation->exitTime)) {
        return; // Don't render if outside entry/exit times
      }

      SkPaint paint;
      paint.setAntiAlias(material->antiAliased);
      paint.setColor(material->color);
      if (material->isFilled && material->isStroked) {
        paint.setStyle(SkPaint::kStrokeAndFill_Style);
      } else if (material->isFilled) {
        paint.setStyle(SkPaint::kFill_Style);
      } else if (material->isStroked) {
        paint.setStyle(SkPaint::kStroke_Style);
      } else {
        paint.setStyle(SkPaint::kFill_Style);
      }
      paint.setStrokeWidth(material->strokeWidth);

      canvas->save();
      canvas->translate(tr.x, tr.y);
      canvas->rotate(tr.rotation * 180 / M_PI);
      canvas->scale(tr.sx, tr.sy);

      switch (shape->kind) {
      case ShapeComponent::Kind::Rectangle: {
        const auto &rectProps =
            std::get<RectangleProperties>(shape->properties);
        canvas->drawRect(
            {0, 0, rectProps.width, rectProps.height}, paint);
        break;
      }
      case ShapeComponent::Kind::Circle: {
        const auto &circleProps = std::get<CircleProperties>(shape->properties);
        canvas->drawCircle(0, 0, circleProps.radius, paint);
        break;
      }
      case ShapeComponent::Kind::Line: {
        const auto &lineProps = std::get<LineProperties>(shape->properties);
        canvas->drawLine(lineProps.x1, lineProps.y1, lineProps.x2, lineProps.y2,
                         paint);
        break;
      }
      case ShapeComponent::Kind::Bezier: {
        const auto &bezierProps = std::get<BezierProperties>(shape->properties);
        SkPath path;
        path.moveTo(bezierProps.x1, bezierProps.y1);
        path.cubicTo(bezierProps.cx1, bezierProps.cy1, bezierProps.cx2,
                     bezierProps.cy2, bezierProps.x2, bezierProps.y2);
        canvas->drawPath(path, paint);
        break;
      }
      case ShapeComponent::Kind::Text: {
        const auto &textProps = std::get<TextProperties>(shape->properties);
        SkFont font;
        font.setSize(textProps.fontSize);
        // TODO: Set font family using SkFontMgr
        canvas->drawString(textProps.text.toStdString().c_str(), 0, 0, font,
                           paint);
        break;
      }
      case ShapeComponent::Kind::Image: {
        const auto &imageProps = std::get<ImageProperties>(shape->properties);
        // For simplicity, load image every time. In a real app, cache this.
        sk_sp<SkData> data =
            SkData::MakeFromFileName(imageProps.filePath.toStdString().c_str());
        // if (data) {
        //   sk_sp<SkImage> image = SkImage::MakeFromEncoded(data);
        //   if (image) {
        //     canvas->drawImage(image.get(), 0, 0, &paint);
        //   }
        // }
        break;
      }
      default:
        /* TODO */
        break;
      }
      canvas->restore();
    });
  }

private:
  Registry &reg_;
};

// -----------------------------------------------------------------------------
//  Convenience: Scene façade to tuck into MainWindow / canvas
// -----------------------------------------------------------------------------
struct Scene {
  Registry reg;
  RenderSystem renderer{reg};
  std::map<ShapeComponent::Kind, int> kindCounters;

  Entity createShape(ShapeComponent::Kind k, float x, float y) {
    Entity e = reg.create();
    reg.emplace<TransformComponent>(e, TransformComponent{x, y});
    ShapeComponent shapeComp{k};
    if (k == ShapeComponent::Kind::Rectangle) {
      shapeComp.properties.emplace<RectangleProperties>();
    } else if (k == ShapeComponent::Kind::Circle) {
      shapeComp.properties.emplace<CircleProperties>();
    } else if (k == ShapeComponent::Kind::Line) {
      shapeComp.properties.emplace<LineProperties>();
    } else if (k == ShapeComponent::Kind::Bezier) {
      shapeComp.properties.emplace<BezierProperties>();
    } else if (k == ShapeComponent::Kind::Text) {
      shapeComp.properties.emplace<TextProperties>();
    } else if (k == ShapeComponent::Kind::Image) {
      shapeComp.properties.emplace<ImageProperties>();
    }
    reg.emplace<ShapeComponent>(e, shapeComp);
    reg.emplace<MaterialComponent>(e);
    reg.emplace<AnimationComponent>(e);
    reg.emplace<AnimationComponent>(e);

    std::string name = ShapeComponent::toString(k);
    int count = kindCounters[k]++;
    if (count > 0) {
      name += "." + std::to_string(count);
    }
    reg.emplace<NameComponent>(e, NameComponent{name});

    return e;
  }
};
