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
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QObject>
#include <QPropertyAnimation>
#include <QVariant>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sol/sol.hpp>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

template <typename Gadget>
static void fillFromJson(Gadget &dst, const QJsonObject &src) {
  const QMetaObject &mo = Gadget::staticMetaObject;
  for (int i = mo.propertyOffset(); i < mo.propertyCount(); ++i) {
    QMetaProperty p = mo.property(i);
    if (src.contains(p.name()))
      p.writeOnGadget(&dst, src[p.name()].toVariant());
  }
}
// -----------------------------------------------------------------------------
//  Core types
// -----------------------------------------------------------------------------
using Entity = uint32_t;
static constexpr Entity kInvalidEntity = 0;

struct Scene;

class ComponentRegistry {
public:
  using Fn = std::function<void(Scene &, Entity, const QJsonValue &)>;

  static ComponentRegistry &instance() {
    static ComponentRegistry inst;
    return inst;
  }

  void registerComponent(const QString &key, Fn fn) {
    map_.insert(key, std::move(fn));
  }

  // Walk every key in the entity-object and dispatch if we know it
  void apply(Scene &scene, Entity e, const QJsonObject &json) const {
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
      auto found = map_.find(it.key());
      if (found != map_.end())
        found.value()(scene, e, it.value());
    }
  }

private:
  QHash<QString, Fn> map_;
};

struct NameComponent {
  std::string name;
};

struct MaterialComponent {
  SkColor color = SK_ColorBLUE;
  bool isFilled = true;
  bool isStroked = false;
  float strokeWidth = 1.0f;
  bool antiAliased = true;
};

struct AnimationComponent {
  float entryTime = 0.0f;
  float exitTime = 5.0f;
};

struct ScriptComponent {
  std::string scriptPath;
  std::string startFunction = "on_start";
  std::string updateFunction = "on_update";
  std::string destroyFunction = "on_destroy";
  sol::table scriptEnv; // Each script gets its own environment/table
};

struct SceneBackgroundComponent {}; // Tag component for the background

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

struct ShapeComponent {
  Q_GADGET
  Q_PROPERTY(Kind kind MEMBER kind)
  Q_PROPERTY(QVariant properties READ getProperties WRITE setProperties)
public:
  enum class Kind { Rectangle, Circle };
  Kind kind;
  std::variant<std::monostate, RectangleProperties, CircleProperties>
      properties; // Add other shape properties here as they are defined

  QVariant getProperties() const {
    return std::visit(
        [](auto &&arg) -> QVariant {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            return QVariant();
          } else {
            QJsonObject obj;
            const QMetaObject metaObject = T::staticMetaObject;
            for (int i = metaObject.propertyOffset();
                 i < metaObject.propertyCount(); ++i) {
              QMetaProperty metaProperty = metaObject.property(i);
              obj[metaProperty.name()] =
                  QJsonValue::fromVariant(metaProperty.readOnGadget(&arg));
            }
            return obj;
          }
        },
        properties);
  }

  void setProperties(const QVariant &value) {
    if (value.canConvert<RectangleProperties>()) {
      properties = value.value<RectangleProperties>();
    } else if (value.canConvert<CircleProperties>()) {
      properties = value.value<CircleProperties>();
    }
  }

  static const char *toString(Kind k) {
    switch (k) {
    case Kind::Rectangle:
      return "Rectangle";
    case Kind::Circle:
      return "Circle";
    }
    return "";
  }
};

// ----------------------------------------------------------------------------
//  Component storage interface (type‑erased)
// ----------------------------------------------------------------------------
class IComponentStorage {
public:
  virtual ~IComponentStorage() = default;
  virtual void remove(Entity e) = 0;
  virtual std::shared_ptr<IComponentStorage> clone() const = 0;
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
    auto it = data_.find(e);
    return it == data_.end() ? nullptr : &it->second;
  }

  void remove(Entity e) override { data_.erase(e); }

  std::shared_ptr<IComponentStorage> clone() const override {
    return std::make_shared<ComponentStorage<T>>(*this);
  }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }

  auto begin() const { return data_.cbegin(); }
  auto end() const { return data_.cend(); }

  auto cbegin() const { return data_.cbegin(); }
  auto cend() const { return data_.cend(); }

private:
  std::unordered_map<Entity, T> data_;
};

// ----------------------------------------------------------------------------
//  Registry: central API the editor interacts with
// ----------------------------------------------------------------------------
class Registry {
public:
  Registry() = default;
  Registry(const Registry &other) : next_(other.next_) {
    for (const auto &pair : other.pools_) {
      pools_.emplace(pair.first, pair.second->clone());
    }
  }

  Registry &operator=(const Registry &other) {
    if (this != &other) { // Prevent self-assignment
      pools_.clear();     // Clear current pools
      for (const auto &pair : other.pools_) {
        pools_.emplace(pair.first, pair.second->clone()); // Deep copy
      }
      next_ = other.next_;
    }
    return *this;
  }

  Entity create() { return next_++; }
  void destroy(Entity e) {
    // Notify ScriptSystem before removing components
    if (scriptDestroyCallback) {
      scriptDestroyCallback(e);
    }
    for (auto &[_, stor] : pools_)
      stor->remove(e);
  }

  // Callback for ScriptSystem to hook into entity destruction
  std::function<void(Entity)> scriptDestroyCallback;

  template <class T> ComponentStorage<T> &storage() {
    const std::type_index idx = typeid(T);
    auto it = pools_.find(idx);
    if (it == pools_.end()) {
      auto ptr = std::make_shared<ComponentStorage<T>>();
      auto raw = ptr.get();
      pools_.emplace(idx, std::move(ptr));
      return *raw;
    }
    return *static_cast<ComponentStorage<T> *>(it->second.get());
  }

  template <class T> const ComponentStorage<T> &storage() const {
    const std::type_index idx = typeid(T);
    auto it = pools_.find(idx);
    if (it == pools_.end()) {
      // This should ideally not happen in a const context if the component is
      // expected to exist. Or, handle error appropriately.
      throw std::runtime_error("Attempted to access non-existent component "
                               "storage in const context.");
    }
    return *static_cast<const ComponentStorage<T> *>(it->second.get());
  }

  template <class T, class... Args> T &emplace(Entity e, Args &&...args) {
    return storage<T>().emplace(e, std::forward<Args>(args)...);
  }

  template <class T> T *get(Entity e) { return storage<T>().get(e); }
  template <class T> const T *get(Entity e) const {
    return storage<T>().get(e);
  }

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

  template <class T, class F> void each(F &&fn) const {
    for (auto &[e, comp] : storage<T>())
      fn(e, comp);
  }

private:
  Entity next_ = 1;
  std::unordered_map<std::type_index, std::shared_ptr<IComponentStorage>>
      pools_;
};

// -----------------------------------------------------------------------------
//  Example components – keep it trivial for now
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//  RenderSystem – draws entities with {Transform, Shape} via Skia
// -----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//  ScriptingEngine: Manages Lua runtime and script execution
// ----------------------------------------------------------------------------
class ScriptingEngine {
public:
  ScriptingEngine(Registry &reg) : lua_(), reg_(reg) {
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

    // Expose C++ types and functions to Lua
    lua_.new_usertype<TransformComponent>(
        "TransformComponent", "x", &TransformComponent::x, "y",
        &TransformComponent::y, "rotation", &TransformComponent::rotation, "sx",
        &TransformComponent::sx, "sy", &TransformComponent::sy);

    lua_.new_usertype<MaterialComponent>(
        "MaterialComponent", "color", &MaterialComponent::color, "isFilled",
        &MaterialComponent::isFilled, "isStroked",
        &MaterialComponent::isStroked, "strokeWidth",
        &MaterialComponent::strokeWidth, "antiAliased",
        &MaterialComponent::antiAliased);

    lua_.new_usertype<Registry>(
        "Registry", "get_transform",
        [&](Registry &self, Entity e) {
          TransformComponent *tc = self.get<TransformComponent>(e);
          if (!tc) {
            std::cerr << "DEBUG: reg_.get<TransformComponent> returned nullptr "
                         "for entity "
                      << e << std::endl;
          }
          return tc;
        },
        "get_material",
        [&](Registry &self, Entity e) {
          return self.get<MaterialComponent>(e);
        },
        "get_script", [&](Entity e) { return reg_.get<ScriptComponent>(e); });
  }

  sol::table loadScript(const std::string &scriptPath, Entity entity) {
    try {
      sol::environment scriptEnv(lua_, sol::create, lua_.globals());
      scriptEnv["entity_id"] = entity;
      scriptEnv["registry"] = std::ref(reg_);
      scriptEnv["print"] =
          lua_["print"]; // Explicitly add print to the script's environment

      // Construct absolute path for the script
      QString appDirPath = QCoreApplication::applicationDirPath();
      QString absoluteScriptPath =
          appDirPath + "/" + QString::fromStdString(scriptPath);

      sol::state_view luaState(lua_);
      luaState.script_file(absoluteScriptPath.toStdString(), scriptEnv);
      return scriptEnv;
    } catch (const sol::error &e) {
      std::cerr << "Lua error loading script " << scriptPath << ": " << e.what()
                << std::endl;
      return sol::nil;
    }
  }

  void callScriptFunction(sol::table &scriptEnv,
                          const std::string &functionName, float dt = 0.0f,
                          float currentTime = 0.0f) {
    if (scriptEnv.valid() && scriptEnv[functionName].valid()) {
      try {
        sol::function func = scriptEnv[functionName];
        func(scriptEnv["entity_id"].get<Entity>(),
             scriptEnv["registry"].get<Registry &>(), dt, currentTime);
      } catch (const sol::error &e) {
        std::cerr << "Lua error in function " << functionName << ": "
                  << e.what() << std::endl;
      }
    }
  }

private:
  sol::state lua_;
  Registry &reg_;
};

// ----------------------------------------------------------------------------
//  ScriptSystem: Executes scripts attached to entities
// ----------------------------------------------------------------------------
class ScriptSystem {
public:
  ScriptSystem(Registry &r, ScriptingEngine &se) : reg_(r), scriptEngine_(se) {
    reg_.scriptDestroyCallback = [&](Entity e) {
      auto *script = reg_.get<ScriptComponent>(e);
      if (script) {
        scriptEngine_.callScriptFunction(script->scriptEnv,
                                         script->destroyFunction);
        // No explicit unload needed for sol2 tables, they are managed by Lua
        // GC
      }
    };
  }

  void tick(float dt, float currentTime) {
    reg_.each<ScriptComponent>([&](Entity ent, ScriptComponent &script) {
      if (!script.scriptEnv.valid()) {
        script.scriptEnv = scriptEngine_.loadScript(script.scriptPath, ent);
        if (script.scriptEnv.valid()) {
          scriptEngine_.callScriptFunction(
              script.scriptEnv, script.startFunction, 0.0f,
              0.0f); // Pass dummy dt and currentTime for start
        }
      }
      if (script.scriptEnv.valid()) {
        scriptEngine_.callScriptFunction(
            script.scriptEnv, script.updateFunction, dt, currentTime);
      }
    });
  }

private:
  Registry &reg_;
  ScriptingEngine &scriptEngine_;
};

#include "render.h"

struct Scene {
  Registry reg;
  ScriptingEngine scriptEngine;
  ScriptSystem scriptSystem;
  RenderSystem renderer;
  std::map<ShapeComponent::Kind, int> kindCounters;

  Scene() : scriptEngine(reg), scriptSystem(reg, scriptEngine), renderer(reg) {}

  Entity createShape(ShapeComponent::Kind k, float x, float y) {
    Entity e = reg.create();
    reg.emplace<TransformComponent>(e, TransformComponent{x, y});
    ShapeComponent shapeComp{k, std::monostate{}};
    if (k == ShapeComponent::Kind::Rectangle) {
      shapeComp.properties.emplace<RectangleProperties>();
    } else if (k == ShapeComponent::Kind::Circle) {
      shapeComp.properties.emplace<CircleProperties>();
    }
    reg.emplace<ShapeComponent>(e, shapeComp);
    reg.emplace<MaterialComponent>(e);
    reg.emplace<AnimationComponent>(e);
    reg.emplace<ScriptComponent>(e);

    std::string name = ShapeComponent::toString(k);
    int count = kindCounters[k]++;
    if (count > 0) {
      name += "." + std::to_string(count);
    }
    reg.emplace<NameComponent>(e, NameComponent{name});

    return e;
  }

  Entity createBackground(float width, float height) {
    Entity e = reg.create();
    reg.emplace<TransformComponent>(e, TransformComponent{0, 0, 0, 1.0f, 1.0f});
    ShapeComponent shapeComp{ShapeComponent::Kind::Rectangle, std::monostate{}};
    shapeComp.properties.emplace<RectangleProperties>(
        RectangleProperties{width, height});
    reg.emplace<ShapeComponent>(e, shapeComp);
    reg.emplace<MaterialComponent>(
        e, MaterialComponent{SkColorSetARGB(255, 22, 22, 22), true, false, 1.0f,
                             true});
    reg.emplace<SceneBackgroundComponent>(e);
    reg.emplace<NameComponent>(e, NameComponent{"Background"});
    return e;
  }

  void clear() {
    // Destroy all entities
    std::vector<Entity> entitiesToDestroy;
    reg.each<TransformComponent>([&](Entity ent, TransformComponent &) {
      entitiesToDestroy.push_back(ent);
    });
    for (Entity ent : entitiesToDestroy) {
      reg.destroy(ent);
    }
    kindCounters.clear();
  }

  QJsonObject serialize() const {
    QJsonObject sceneObject;
    QJsonArray entitiesArray;

    reg.each<TransformComponent>([&](Entity ent, const TransformComponent &tr) {
      QJsonObject entityObject;
      entityObject["id"] = (qint64)ent;

      // Serialize TransformComponent
      QJsonObject transformObject;
      transformObject["x"] = tr.x;
      transformObject["y"] = tr.y;
      transformObject["rotation"] = tr.rotation;
      transformObject["sx"] = tr.sx;
      transformObject["sy"] = tr.sy;
      entityObject["TransformComponent"] = transformObject;

      // Serialize NameComponent
      if (auto *name = reg.get<NameComponent>(ent)) {
        entityObject["NameComponent"] = name->name.c_str();
      }

      // Serialize MaterialComponent
      if (auto *material = reg.get<MaterialComponent>(ent)) {
        QJsonObject materialObject;
        materialObject["color"] = (qint64)material->color;
        materialObject["isFilled"] = material->isFilled;
        materialObject["isStroked"] = material->isStroked;
        materialObject["strokeWidth"] = material->strokeWidth;
        materialObject["antiAliased"] = material->antiAliased;
        entityObject["MaterialComponent"] = materialObject;
      }

      // Serialize AnimationComponent
      if (auto *animation = reg.get<AnimationComponent>(ent)) {
        QJsonObject animationObject;
        animationObject["entryTime"] = animation->entryTime;
        animationObject["exitTime"] = animation->exitTime;
        entityObject["AnimationComponent"] = animationObject;
      }

      // Serialize ScriptComponent
      if (auto *script = reg.get<ScriptComponent>(ent)) {
        QJsonObject scriptObject;
        scriptObject["scriptPath"] = script->scriptPath.c_str();
        scriptObject["startFunction"] = script->startFunction.c_str();
        scriptObject["updateFunction"] = script->updateFunction.c_str();
        scriptObject["destroyFunction"] = script->destroyFunction.c_str();
        entityObject["ScriptComponent"] = scriptObject;
      }

      // Serialize SceneBackgroundComponent
      if (reg.has<SceneBackgroundComponent>(ent)) {
        entityObject["SceneBackgroundComponent"] = true;
      }

      // Serialize ShapeComponent
      if (auto *shape = reg.get<ShapeComponent>(ent)) {
        QJsonObject shapeObject;
        shapeObject["kind"] = (int)shape->kind;
        shapeObject["properties"] = shape->getProperties().toJsonObject();
        entityObject["ShapeComponent"] = shapeObject;
      }

      entitiesArray.append(entityObject);
    });

    sceneObject["entities"] = entitiesArray;
    return sceneObject;
  }

  void deserialize(const QJsonObject &sceneObject) {
    clear(); // wipe current scene

    if (!sceneObject.contains("entities") || !sceneObject["entities"].isArray())
      return;

    const QJsonArray arr = sceneObject["entities"].toArray();
    for (const QJsonValue &v : arr) {
      if (!v.isObject())
        continue;

      Entity e = reg.create();
      ComponentRegistry::instance().apply(*this, e, v.toObject());
    }
  }
};

#define REGISTER_COMPONENT(KEY, ...)                                           \
  namespace {                                                                  \
  const bool _registered_##KEY = [] {                                          \
    ComponentRegistry::instance().registerComponent(QStringLiteral(#KEY),      \
                                                    __VA_ARGS__);              \
    return true;                                                               \
  }();                                                                         \
  }

REGISTER_COMPONENT(NameComponent, [](Scene &scene, Entity ent,
                                     const QJsonValue &val) {
  if (!val.isString())
    return; // defensive
  QString base = val.toString();
  std::string unique = base.toStdString();

  int counter = 1;
  bool exists = true;
  while (exists) {
    exists = false;
    scene.reg.each<NameComponent>([&](Entity, NameComponent &nc) {
      if (nc.name == unique)
        exists = true;
    });
    if (exists)
      unique = base.toStdString() + "." + std::to_string(counter++);
  }
  scene.reg.emplace<NameComponent>(ent, NameComponent{unique});
});

REGISTER_COMPONENT(TransformComponent, [](Scene &scene, Entity ent,
                                          const QJsonValue &val) {
  const QJsonObject o = val.toObject();
  scene.reg.emplace<TransformComponent>(
      ent, TransformComponent{static_cast<float>(o["x"].toDouble()) + 10.0f,
                              static_cast<float>(o["y"].toDouble()) + 10.0f,
                              static_cast<float>(o["rotation"].toDouble()),
                              static_cast<float>(o["sx"].toDouble()),
                              static_cast<float>(o["sy"].toDouble())});
});

REGISTER_COMPONENT(MaterialComponent, [](Scene &scene, Entity ent,
                                         const QJsonValue &val) {
  const QJsonObject o = val.toObject();
  scene.reg.emplace<MaterialComponent>(
      ent, MaterialComponent{
               static_cast<SkColor>(o["color"].toVariant().toULongLong()),
               o["isFilled"].toBool(), o["isStroked"].toBool(),
               static_cast<float>(o["strokeWidth"].toDouble()),
               o["antiAliased"].toBool()});
});

REGISTER_COMPONENT(SceneBackgroundComponent,
                   [](Scene &scene, Entity ent, const QJsonValue &) {
                     scene.reg.emplace<SceneBackgroundComponent>(ent);
                   });

#define SHAPE_CASE(KEY, TYPE)                                                  \
  case ShapeComponent::Kind::KEY: {                                            \
    TYPE tmp;                                                                  \
    fillFromJson(tmp, props);                                                  \
    sc.properties = tmp;                                                       \
    break;                                                                     \
  }

REGISTER_COMPONENT(ShapeComponent, [](Scene &scene, Entity ent,
                                      const QJsonValue &val) {
  const QJsonObject root = val.toObject();
  auto kind = static_cast<ShapeComponent::Kind>(root["kind"].toInt());

  ShapeComponent sc{kind, std::monostate{}};
  if (root.contains("properties")) {
    const QJsonObject props = root["properties"].toObject();
    switch (kind) {
      SHAPE_CASE(Rectangle, RectangleProperties)
      SHAPE_CASE(Circle, CircleProperties)
    }
  }
  scene.reg.emplace<ShapeComponent>(ent, sc);
});
