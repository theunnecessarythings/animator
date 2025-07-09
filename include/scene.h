#pragma once

#include "ecs.h"
#include "render.h"
#include "scripting.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>
#include <unordered_map>

class Scene {
public:
  Scene()
      : world(std::make_unique<flecs::world>()), scriptingEngine(*world),
        scriptSystem(*world, scriptingEngine), renderer(*world, scriptSystem) {
    world->set<TimeSingleton>({0.f});
  }

  ~Scene() { world.reset(); }

  // ---------------------------------------------------------------------
  //  Public helpers used by the editor
  // ---------------------------------------------------------------------
  Entity createShape(const std::string &kind, float x, float y);

  Entity createBackground(float width, float height);

  // ---------------------------------------------------------------------
  //  Frame tick helpers
  // ---------------------------------------------------------------------
  void update(float dt, float timelineSeconds);

  void draw(SkCanvas *canvas, float timelineSeconds);

  // ---------------------------------------------------------------------
  //  Serialization helpers
  // ---------------------------------------------------------------------
  QJsonObject serialize() const;

  void deserialize(const QJsonObject &root);

  void clear();

  ScriptSystem &getScriptSystem() { return scriptSystem; }

  RenderSystem &getRenderer() { return renderer; }

  // Expose world for editor loops ---------------------------------------
  flecs::world &ecs() { return *world; }
  const flecs::world &ecs() const { return *world; }

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
  std::unique_ptr<flecs::world> world;

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
