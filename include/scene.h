#pragma once

#include "ecs.h"
#include "render.h"
#include "scripting.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <unordered_map>

class Scene {
public:
  Scene()
      : scriptingEngine(world), scriptSystem(world, scriptingEngine),
        renderer(world) {
    world.set<TimeSingleton>({0.f});
  }

  // ---------------------------------------------------------------------
  //  Public helpers used by the editor
  // ---------------------------------------------------------------------
  Entity createShape(const std::string &kind, float x, float y) {
    Entity e = world.entity();

    e.set<TransformComponent>({x, y});
    e.set<ShapeComponent>(ShapeFactory::create(kind));
    e.set<MaterialComponent>({});

    std::string name = uniqueName(kind);
    e.set<NameComponent>({name});
    return e;
  }

  Entity createBackground(float width, float height) {
    Entity e = world.entity("Background");
    e.set<NameComponent>({"Background"});
    e.set<TransformComponent>({0, 0, 0, 1.f, 1.f});
    e.add<SceneBackgroundComponent>();

    auto rect = std::make_unique<RectangleShape>();
    QJsonObject props;
    props["Width"] = width;
    props["Height"] = height;
    rect->deserialize(props);

    e.set<ShapeComponent>({std::move(rect)});
    e.set<MaterialComponent>(
        {SkColorSetARGB(255, 22, 22, 22), true, false, 1.f, true});
    return e;
  }

  // ---------------------------------------------------------------------
  //  Frame tick helpers
  // ---------------------------------------------------------------------
  void update(float dt, float timelineSeconds) {
    world.get_mut<TimeSingleton>().time = timelineSeconds;
    world.progress(dt);
  }

  void draw(SkCanvas *canvas, float timelineSeconds) {
    renderer.render(canvas, timelineSeconds);
  }

  // ---------------------------------------------------------------------
  //  Serialization helpers
  // ---------------------------------------------------------------------
  QJsonObject serialize() const {
    QJsonObject obj;
    QJsonArray arr;
    world.each<const TransformComponent>(
        [&](flecs::entity e, const TransformComponent &) {
          QJsonObject ent;
          ent["id"] = static_cast<qint64>(e.id());

          if (e.has<NameComponent>()) {
            auto &n = e.get<NameComponent>();
            ent["NameComponent"] = QString::fromStdString(n.name);
          }
          if (e.has<TransformComponent>()) {
            auto &t = e.get<TransformComponent>();
            QJsonObject j;
            j["x"] = t.x;
            j["y"] = t.y;
            j["rotation"] = t.rotation;
            j["sx"] = t.sx;
            j["sy"] = t.sy;
            ent["TransformComponent"] = j;
          }
          if (e.has<MaterialComponent>()) {
            auto &m = e.get<MaterialComponent>();
            QJsonObject j;
            j["color"] = static_cast<qint64>(m.color);
            j["isFilled"] = m.isFilled;
            j["isStroked"] = m.isStroked;
            j["strokeWidth"] = m.strokeWidth;
            j["antiAliased"] = m.antiAliased;
            ent["MaterialComponent"] = j;
          }
          if (e.has<AnimationComponent>()) {
            auto &a = e.get<AnimationComponent>();
            QJsonObject j;
            j["entryTime"] = a.entryTime;
            j["exitTime"] = a.exitTime;
            ent["AnimationComponent"] = j;
          }
          if (e.has<ScriptComponent>()) {
            auto &s = e.get<ScriptComponent>();
            QJsonObject j;
            j["scriptPath"] = QString::fromStdString(s.scriptPath);
            j["startFunction"] = QString::fromStdString(s.startFunction);
            j["updateFunction"] = QString::fromStdString(s.updateFunction);
            j["destroyFunction"] = QString::fromStdString(s.destroyFunction);
            ent["ScriptComponent"] = j;
          }
          if (e.has<SceneBackgroundComponent>())
            ent["SceneBackgroundComponent"] = true;
          if (e.has<ShapeComponent>()) {
            auto &sh = e.get<ShapeComponent>();
            if (!sh.shape)
              return; // Skip entities without a shape
            QJsonObject j;
            j["kind"] = sh.shape->getKindName();
            j["properties"] = sh.shape->serialize();
            ent["ShapeComponent"] = j;
          }
          arr.append(ent);
        });
    obj["entities"] = arr;
    return obj;
  }

  void deserialize(const QJsonObject &root) {
    clear();
    if (!root.contains("entities") || !root["entities"].isArray())
      return;

    const QJsonArray arr = root["entities"].toArray();
    qDebug() << "Deserializing" << arr.size() << "entities.";

    for (const auto &v : arr)
      if (v.isObject()) {
        const QJsonObject eobj = v.toObject();
        Entity e = world.entity();

        // Name -------------------------------------------------------------
        if (eobj.contains("NameComponent"))
          e.set<NameComponent>(
              {eobj["NameComponent"].toString().toStdString()});

        // Transform --------------------------------------------------------
        if (eobj.contains("TransformComponent")) {
          const QJsonObject j = eobj["TransformComponent"].toObject();
          e.set<TransformComponent>(
              {static_cast<float>(j["x"].toDouble()),
               static_cast<float>(j["y"].toDouble()),
               static_cast<float>(j["rotation"].toDouble()),
               static_cast<float>(j["sx"].toDouble()),
               static_cast<float>(j["sy"].toDouble())});
        }

        // Material ---------------------------------------------------------
        if (eobj.contains("MaterialComponent")) {
          const QJsonObject j = eobj["MaterialComponent"].toObject();
          e.set<MaterialComponent>(
              {static_cast<SkColor>(j["color"].toVariant().toULongLong()),
               j["isFilled"].toBool(), j["isStroked"].toBool(),
               static_cast<float>(j["strokeWidth"].toDouble()),
               j["antiAliased"].toBool()});
        }

        // Animation --------------------------------------------------------
        if (eobj.contains("AnimationComponent")) {
          const QJsonObject j = eobj["AnimationComponent"].toObject();
          e.set<AnimationComponent>(
              {static_cast<float>(j["entryTime"].toDouble()),
               static_cast<float>(j["exitTime"].toDouble())});
        }

        // Script -----------------------------------------------------------
        if (eobj.contains("ScriptComponent")) {
          const QJsonObject j = eobj["ScriptComponent"].toObject();
          e.set<ScriptComponent>({j["scriptPath"].toString().toStdString(),
                                  j["startFunction"].toString().toStdString(),
                                  j["updateFunction"].toString().toStdString(),
                                  j["destroyFunction"].toString().toStdString(),
                                  {}}); // env filled on OnAdd
        }

        // Tag --------------------------------------------------------------
        if (eobj.contains("SceneBackgroundComponent"))
          e.add<SceneBackgroundComponent>();

        // Shape ------------------------------------------------------------
        if (eobj.contains("ShapeComponent")) {
          const QJsonObject j = eobj["ShapeComponent"].toObject();
          std::string kind = j["kind"].toString().toStdString();
          auto shapePtr = ShapeFactory::create(kind);
          if (shapePtr && j.contains("properties"))
            shapePtr->deserialize(j["properties"].toObject());
          e.set<ShapeComponent>({std::move(shapePtr)});
        }
      }
  }

  void clear() {
    world.reset();
    kindCounters.clear();
  }

  ScriptSystem &getScriptSystem() { return scriptSystem; }

  // Expose world for editor loops ---------------------------------------
  flecs::world &ecs() { return world; }
  const flecs::world &ecs() const { return world; }

private:
  // Unique‑name helper
  std::string uniqueName(const std::string &base) {
    int &counter = kindCounters[base];
    std::string n = base;
    if (counter > 0)
      n += "." + std::to_string(counter);
    ++counter;
    return n;
  }

  // Flecs data ----------------------------------------------------------
  flecs::world world;

  struct TimeSingleton {
    float time = 0.f;
  };

  // Sub‑systems ---------------------------------------------------------
  ScriptingEngine scriptingEngine;
  ScriptSystem scriptSystem;
  RenderSystem renderer;

  // Name uniqueness map -------------------------------------------------
  std::unordered_map<std::string, int> kindCounters;
};
