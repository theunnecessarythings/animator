#pragma once
//
// #define FLECS_CPP
// #include <flecs.h>
//
// #include "include/codec/SkCodec.h"
// #include "include/core/SkBitmap.h"
// #include "include/core/SkCanvas.h"
// #include "include/core/SkColor.h"
// #include "include/core/SkFont.h"
// #include "include/core/SkFontMgr.h"
// #include "include/core/SkImage.h"
// #include "include/core/SkPaint.h"
// #include "include/core/SkPath.h"
//
// #include <QCoreApplication>
// #include <QDebug>
// #include <QJsonArray>
// #include <QJsonObject>
// #include <QMetaObject>
// #include <QMetaProperty>
// #include <QMetaType>
// #include <QObject>
// #include <QPropertyAnimation>
// #include <QVariant>
//
// #include <cassert>
// #include <cstdint>
// #include <functional>
// #include <iostream>
// #include <map>
// #include <memory>
// #include <sol/sol.hpp>
// #include <string>
// #include <type_traits>
// #include <unordered_map>
// #include <utility>
// #include <vector>
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// // helpers
// //
// ──────────────────────────────────────────────────────────────────────────────
// template <typename Gadget>
// static void fillFromJson(Gadget &dst, const QJsonObject &src) {
//   const QMetaObject &mo = Gadget::staticMetaObject;
//   for (int i = mo.propertyOffset(); i < mo.propertyCount(); ++i) {
//     QMetaProperty p = mo.property(i);
//     if (src.contains(p.name()))
//       p.writeOnGadget(&dst, src[p.name()].toVariant());
//   }
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  Component types
// //
// ──────────────────────────────────────────────────────────────────────────────
// using Entity = flecs::id_t;
// static constexpr Entity kInvalidEntity = 0;
//
// struct NameComponent {
//   std::string name;
// };
//
// struct MaterialComponent {
//   SkColor color = SK_ColorBLUE;
//   bool isFilled = true;
//   bool isStroked = false;
//   float strokeWidth = 1.f;
//   bool antiAliased = true;
// };
//
// struct AnimationComponent {
//   float entryTime = 0.f, exitTime = 5.f;
// };
//
// struct ScriptComponent {
//   std::string scriptPath;
//   std::string startFunction = "on_start";
//   std::string updateFunction = "on_update";
//   std::string destroyFunction = "on_destroy";
//   sol::table scriptEnv; // each script gets its own Lua env
// };
//
// struct SceneBackgroundComponent {}; // tag
//
// struct TransformComponent {
//   float x = 0.f, y = 0.f;
//   float rotation = 0.f;     // radians
//   float sx = 1.f, sy = 1.f; // scale
// };
//
// #include "shapes.h"
//
// struct ShapeComponent {
//   std::unique_ptr<Shape> shape;
//   ShapeComponent() = default;
//   ShapeComponent(ShapeComponent &&) noexcept = default;
//   ShapeComponent &operator=(ShapeComponent &&) noexcept = default;
//
//   ShapeComponent(const ShapeComponent &other) {
//     if (other.shape)
//       shape = other.shape->clone();
//   }
//   ShapeComponent &operator=(const ShapeComponent &other) {
//     if (this != &other)
//       shape = other.shape ? other.shape->clone() : nullptr;
//     return *this;
//   }
// };
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  Runtime-component registry for JSON loading
// //
// ──────────────────────────────────────────────────────────────────────────────
// struct Scene; // fwd
//
// class ComponentRegistry {
// public:
//   using Fn = std::function<void(Scene &, Entity, const QJsonValue &)>;
//
//   static ComponentRegistry &instance() {
//     static ComponentRegistry inst;
//     return inst;
//   }
//
//   void registerComponent(const QString &key, Fn fn) {
//     map_.insert(key, std::move(fn));
//   }
//
//   void apply(Scene &scene, Entity e, const QJsonObject &json) const {
//     for (auto it = json.constBegin(); it != json.constEnd(); ++it)
//       if (auto found = map_.find(it.key()); found != map_.end())
//         found.value()(scene, e, it.value());
//   }
//
// private:
//   QHash<QString, Fn> map_;
// };
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  Registry  → thin wrapper around flecs::world
// //
// ──────────────────────────────────────────────────────────────────────────────
// class Registry {
// public:
//   Registry() = default;
//   Registry(const Registry &) = delete;
//   Registry &operator=(const Registry &) = delete;
//
//   // Entity life-cycle
//   ------------------------------------------------------- Entity create() {
//     flecs::entity fe = world_.entity();
//     Entity id = fe.id();
//     idMap_[id] = fe;
//     return id;
//   }
//
//   void destroy(Entity id) {
//     if (scriptDestroyCallback)
//       scriptDestroyCallback(id);
//
//     if (auto it = idMap_.find(id); it != idMap_.end()) {
//       it->second.destruct();
//       idMap_.erase(it);
//     }
//   }
//
//   // Component access
//   -------------------------------------------------------- template <class T,
//   class... Args> T &emplace(Entity id, Args &&...args) {
//     flecs::entity e = fetch(id);
//
//     if constexpr (std::is_empty_v<T>) {
//       e.add<T>();
//       static T dummy{};
//       return dummy;
//     } else {
//       e.set<T>({std::forward<Args>(args)...});
//
//       if constexpr (std::is_pointer_v<decltype(e.get_mut<T>())>)
//         return *e.get_mut<T>();
//       else
//         return e.get_mut<T>();
//     }
//   }
//
//   template <class T> T *get(Entity id) {
//     flecs::entity e = fetch(id);
//     if (!e.has<T>()) // <-- guard
//       return nullptr;
//
//     if constexpr (std::is_pointer_v<decltype(e.get_mut<T>())>)
//       return e.get_mut<T>(); // Flecs ≤3
//     else
//       return &e.get_mut<T>(); // Flecs 4
//   }
//
//   // const overload
//   template <class T> const T *get(Entity id) const {
//     flecs::entity e = fetch(id);
//     if (!e.has<T>()) // <-- guard
//       return nullptr;
//
//     if constexpr (std::is_pointer_v<decltype(e.get<T>())>)
//       return e.get<T>(); // pointer
//     else
//       return &e.get<T>(); // reference → pointer
//   }
//   template <class T> bool has(Entity id) const {
//     if (auto it = idMap_.find(id); it != idMap_.end())
//       return it->second.has<T>();
//     return false;
//   }
//
//   // iteration
//   --------------------------------------------------------------- template
//   <class T, class F> void each(F &&fn) {
//     world_.each<T>([&](flecs::entity e, T &comp) { fn(e.id(), comp); });
//   }
//   template <class T, class F> void each(F &&fn) const {
//     const_cast<flecs::world &>(world_) // C API trait; safe here
//         .each<T>([&](flecs::entity e, const T &comp) { fn(e.id(), comp); });
//   }
//
//   // Lua script hook
//   ---------------------------------------------------------
//   std::function<void(Entity)> scriptDestroyCallback;
//
//   flecs::world &world() { return world_; }
//   const flecs::world &world() const { return world_; }
//
// private:
//   flecs::entity fetch(Entity id) const {
//     auto it = idMap_.find(id);
//     assert(it != idMap_.end() && "Invalid Entity handle");
//     return it->second;
//   }
//
//   mutable flecs::world world_;
//   std::unordered_map<Entity, flecs::entity> idMap_;
// };
//
// #include "render.h"
//
// //
// ----------------------------------------------------------------------------
// //  ScriptingEngine: Manages Lua runtime and script execution
// //
// ----------------------------------------------------------------------------
// class ScriptingEngine {
// public:
//   ScriptingEngine(Registry &reg) : lua_(), reg_(reg) {
//     lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
//
//     // Expose C++ types and functions to Lua
//     lua_.new_usertype<TransformComponent>(
//         "TransformComponent", "x", &TransformComponent::x, "y",
//         &TransformComponent::y, "rotation", &TransformComponent::rotation,
//         "sx", &TransformComponent::sx, "sy", &TransformComponent::sy);
//
//     lua_.new_usertype<MaterialComponent>(
//         "MaterialComponent", "color", &MaterialComponent::color, "isFilled",
//         &MaterialComponent::isFilled, "isStroked",
//         &MaterialComponent::isStroked, "strokeWidth",
//         &MaterialComponent::strokeWidth, "antiAliased",
//         &MaterialComponent::antiAliased);
//
//     lua_.new_usertype<Registry>(
//         "Registry", "get_transform",
//         [&](Registry &self, Entity e) {
//           TransformComponent *tc = self.get<TransformComponent>(e);
//           if (!tc) {
//             std::cerr << "DEBUG: reg_.get<TransformComponent> returned
//             nullptr "
//                          "for entity "
//                       << e << std::endl;
//           }
//           return tc;
//         },
//         "get_material",
//         [&](Registry &self, Entity e) {
//           return self.get<MaterialComponent>(e);
//         },
//         "get_script", [&](Entity e) { return reg_.get<ScriptComponent>(e);
//         });
//   }
//
//   sol::table loadScript(const std::string &scriptPath, Entity entity) {
//     try {
//       sol::environment scriptEnv(lua_, sol::create, lua_.globals());
//       scriptEnv["entity_id"] = entity;
//       scriptEnv["registry"] = std::ref(reg_);
//       scriptEnv["print"] =
//           lua_["print"]; // Explicitly add print to the script's environment
//
//       // Construct absolute path for the script
//       QString appDirPath = QCoreApplication::applicationDirPath();
//       QString absoluteScriptPath =
//           appDirPath + "/" + QString::fromStdString(scriptPath);
//
//       sol::state_view luaState(lua_);
//       luaState.script_file(absoluteScriptPath.toStdString(), scriptEnv);
//       return scriptEnv;
//     } catch (const sol::error &e) {
//       std::cerr << "Lua error loading script " << scriptPath << ": " <<
//       e.what()
//                 << std::endl;
//       return sol::nil;
//     }
//   }
//
//   void callScriptFunction(sol::table &scriptEnv,
//                           const std::string &functionName, float dt = 0.0f,
//                           float currentTime = 0.0f) {
//     if (scriptEnv.valid() && scriptEnv[functionName].valid()) {
//       try {
//         sol::function func = scriptEnv[functionName];
//         func(scriptEnv["entity_id"].get<Entity>(),
//              scriptEnv["registry"].get<Registry &>(), dt, currentTime);
//       } catch (const sol::error &e) {
//         std::cerr << "Lua error in function " << functionName << ": "
//                   << e.what() << std::endl;
//       }
//     }
//   }
//
// private:
//   sol::state lua_;
//   Registry &reg_;
// };
//
// //
// ----------------------------------------------------------------------------
// //  ScriptSystem: Executes scripts attached to entities
// //
// ----------------------------------------------------------------------------
// class ScriptSystem {
// public:
//   ScriptSystem(Registry &r, ScriptingEngine &se) : reg_(r), scriptEngine_(se)
//   {
//     reg_.scriptDestroyCallback = [&](Entity e) {
//       auto *script = reg_.get<ScriptComponent>(e);
//       if (script) {
//         scriptEngine_.callScriptFunction(script->scriptEnv,
//                                          script->destroyFunction);
//         // No explicit unload needed for sol2 tables, they are managed by Lua
//         // GC
//       }
//     };
//   }
//
//   void tick(float dt, float currentTime) {
//     reg_.each<ScriptComponent>([&](Entity ent, ScriptComponent &script) {
//       if (!script.scriptEnv.valid()) {
//         script.scriptEnv = scriptEngine_.loadScript(script.scriptPath, ent);
//         if (script.scriptEnv.valid()) {
//           scriptEngine_.callScriptFunction(
//               script.scriptEnv, script.startFunction, 0.0f,
//               0.0f); // Pass dummy dt and currentTime for start
//         }
//       }
//       if (script.scriptEnv.valid()) {
//         scriptEngine_.callScriptFunction(
//             script.scriptEnv, script.updateFunction, dt, currentTime);
//       }
//     });
//   }
//
// private:
//   Registry &reg_;
//   ScriptingEngine &scriptEngine_;
// };
//
// struct Scene {
//   Registry reg;
//   ScriptingEngine scriptEngine;
//   ScriptSystem scriptSystem;
//   RenderSystem renderer;
//   std::map<std::string, int> kindCounters;
//
//   Scene() : scriptEngine(reg), scriptSystem(reg, scriptEngine), renderer(reg)
//   {}
//
//   Entity createShape(const std::string &kind, float x, float y) {
//     Entity e = reg.create();
//     reg.emplace<TransformComponent>(e, TransformComponent{x, y});
//
//     ShapeComponent shapeComp;
//     shapeComp.shape = ShapeFactory::create(kind);
//     if (!shapeComp.shape)
//       return kInvalidEntity; // Could not create shape
//
//     reg.emplace<ShapeComponent>(e, std::move(shapeComp));
//     reg.emplace<MaterialComponent>(e);
//     reg.emplace<AnimationComponent>(e);
//     reg.emplace<ScriptComponent>(e);
//
//     std::string name = kind;
//     int count = kindCounters[kind]++;
//     if (count > 0) {
//       name += "." + std::to_string(count);
//     }
//     reg.emplace<NameComponent>(e, NameComponent{name});
//
//     return e;
//   }
//
//   Entity createBackground(float width, float height) {
//     Entity e = reg.create();
//     reg.emplace<TransformComponent>(e, TransformComponent{0, 0,
//     0, 1.0f, 1.0f});
//
//     // Create a RectangleShape polymorphically
//     ShapeComponent shapeComp;
//     shapeComp.shape = std::make_unique<RectangleShape>();
//
//     // Deserialize its properties to set width and height
//     QJsonObject props;
//     props["width"] = width;
//     props["height"] = height;
//     shapeComp.shape->deserialize(props);
//
//     reg.emplace<ShapeComponent>(e, std::move(shapeComp));
//     reg.emplace<MaterialComponent>(
//         e, MaterialComponent{SkColorSetARGB(255, 22, 22, 22), true,
//         false, 1.0f,
//                              true});
//     reg.emplace<SceneBackgroundComponent>(e);
//     reg.emplace<NameComponent>(e, NameComponent{"Background"});
//     return e;
//   }
//
//   void clear() {
//     // Destroy all entities
//     std::vector<Entity> entitiesToDestroy;
//     reg.each<TransformComponent>([&](Entity ent, TransformComponent &) {
//       entitiesToDestroy.push_back(ent);
//     });
//     for (Entity ent : entitiesToDestroy) {
//       reg.destroy(ent);
//     }
//     kindCounters.clear();
//   }
//
//   QJsonObject serialize() const {
//     QJsonObject sceneObject;
//     QJsonArray entitiesArray;
//
//     reg.each<TransformComponent>([&](Entity ent, const TransformComponent
//     &tr) {
//       QJsonObject entityObject;
//       entityObject["id"] = (qint64)ent;
//
//       // Serialize TransformComponent
//       QJsonObject transformObject;
//       transformObject["x"] = tr.x;
//       transformObject["y"] = tr.y;
//       transformObject["rotation"] = tr.rotation;
//       transformObject["sx"] = tr.sx;
//       transformObject["sy"] = tr.sy;
//       entityObject["TransformComponent"] = transformObject;
//
//       // Serialize NameComponent
//       if (auto *name = reg.get<NameComponent>(ent)) {
//         entityObject["NameComponent"] = name->name.c_str();
//       }
//
//       // Serialize MaterialComponent
//       if (auto *material = reg.get<MaterialComponent>(ent)) {
//         QJsonObject materialObject;
//         materialObject["color"] = (qint64)material->color;
//         materialObject["isFilled"] = material->isFilled;
//         materialObject["isStroked"] = material->isStroked;
//         materialObject["strokeWidth"] = material->strokeWidth;
//         materialObject["antiAliased"] = material->antiAliased;
//         entityObject["MaterialComponent"] = materialObject;
//       }
//
//       // Serialize AnimationComponent
//       if (auto *animation = reg.get<AnimationComponent>(ent)) {
//         QJsonObject animationObject;
//         animationObject["entryTime"] = animation->entryTime;
//         animationObject["exitTime"] = animation->exitTime;
//         entityObject["AnimationComponent"] = animationObject;
//       }
//
//       // Serialize ScriptComponent
//       if (auto *script = reg.get<ScriptComponent>(ent)) {
//         QJsonObject scriptObject;
//         scriptObject["scriptPath"] = script->scriptPath.c_str();
//         scriptObject["startFunction"] = script->startFunction.c_str();
//         scriptObject["updateFunction"] = script->updateFunction.c_str();
//         scriptObject["destroyFunction"] = script->destroyFunction.c_str();
//         entityObject["ScriptComponent"] = scriptObject;
//       }
//
//       // Serialize SceneBackgroundComponent
//       if (reg.has<SceneBackgroundComponent>(ent)) {
//         entityObject["SceneBackgroundComponent"] = true;
//       }
//
//       // Serialize ShapeComponent
//       if (auto *sh = reg.get<ShapeComponent>(ent)) {
//         if (sh->shape) {
//           QJsonObject shapeObject;
//           shapeObject["kind"] = sh->shape->getKindName();
//           shapeObject["properties"] = sh->shape->serialize();
//           entityObject["ShapeComponent"] = shapeObject;
//         }
//       }
//
//       entitiesArray.append(entityObject);
//     });
//
//     sceneObject["entities"] = entitiesArray;
//     return sceneObject;
//   }
//
//   void deserialize(const QJsonObject &sceneObject) {
//     clear(); // wipe current scene
//
//     if (!sceneObject.contains("entities") ||
//     !sceneObject["entities"].isArray())
//       return;
//
//     const QJsonArray arr = sceneObject["entities"].toArray();
//     for (const QJsonValue &v : arr) {
//       if (!v.isObject())
//         continue;
//
//       Entity e = reg.create();
//       ComponentRegistry::instance().apply(*this, e, v.toObject());
//     }
//   }
// };
//
// #define REGISTER_COMPONENT(KEY, ...) \
//   namespace { \
//   const bool _registered_##KEY = [] { \
//     ComponentRegistry::instance().registerComponent(QStringLiteral(#KEY), \
//                                                     __VA_ARGS__); \
//     return true; \
//   }(); \
//   }
//
// REGISTER_COMPONENT(NameComponent, [](Scene &scene, Entity ent,
//                                      const QJsonValue &val) {
//   if (!val.isString())
//     return; // defensive
//   QString base = val.toString();
//   std::string unique = base.toStdString();
//
//   int counter = 1;
//   bool exists = true;
//   while (exists) {
//     exists = false;
//     scene.reg.each<NameComponent>([&](Entity, NameComponent &nc) {
//       if (nc.name == unique)
//         exists = true;
//     });
//     if (exists)
//       unique = base.toStdString() + "." + std::to_string(counter++);
//   }
//   scene.reg.emplace<NameComponent>(ent, NameComponent{unique});
// });
//
// REGISTER_COMPONENT(TransformComponent, [](Scene &scene, Entity ent,
//                                           const QJsonValue &val) {
//   const QJsonObject o = val.toObject();
//   scene.reg.emplace<TransformComponent>(
//       ent, TransformComponent{static_cast<float>(o["x"].toDouble()) + 10.0f,
//                               static_cast<float>(o["y"].toDouble()) + 10.0f,
//                               static_cast<float>(o["rotation"].toDouble()),
//                               static_cast<float>(o["sx"].toDouble()),
//                               static_cast<float>(o["sy"].toDouble())});
// });
//
// REGISTER_COMPONENT(MaterialComponent, [](Scene &scene, Entity ent,
//                                          const QJsonValue &val) {
//   const QJsonObject o = val.toObject();
//   scene.reg.emplace<MaterialComponent>(
//       ent, MaterialComponent{
//                static_cast<SkColor>(o["color"].toVariant().toULongLong()),
//                o["isFilled"].toBool(), o["isStroked"].toBool(),
//                static_cast<float>(o["strokeWidth"].toDouble()),
//                o["antiAliased"].toBool()});
// });
//
// REGISTER_COMPONENT(SceneBackgroundComponent,
//                    [](Scene &scene, Entity ent, const QJsonValue &) {
//                      scene.reg.emplace<SceneBackgroundComponent>(ent);
//                    });
//
// #define SHAPE_CASE(KEY, TYPE) \
//   case ShapeComponent::Kind::KEY: { \
//     TYPE tmp; \
//     fillFromJson(tmp, props); \
//     sc.properties = tmp; \
//     break; \
//   }
//
// REGISTER_COMPONENT(ShapeComponent,
//                    [](Scene &scene, Entity ent, const QJsonValue &val) {
//                      const QJsonObject root = val.toObject();
//                      std::string kind =
//                      root["kind"].toString().toStdString();
//
//                      ShapeComponent sc;
//                      sc.shape = ShapeFactory::create(kind);
//
//                      if (sc.shape && root.contains("properties")) {
//                        sc.shape->deserialize(root["properties"].toObject());
//                      }
//                      scene.reg.emplace<ShapeComponent>(ent, std::move(sc));
//                    });

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
// constexpr Entity kInvalidEntity = {};
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
