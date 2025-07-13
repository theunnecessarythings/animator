#pragma once

#include "ecs.h"
#include "qglobal.h"
#include "render.h"
#include "scripting.h"

#include "cpp_script_interface.h"

#include <dlfcn.h>
#include <iostream>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>
#include <unordered_map>

class SkiaCanvasWidget;

class Scene {
public:
  Scene(SkiaCanvasWidget *canvas);

  ~Scene() { world.reset(); }

  // ---------------------------------------------------------------------
  //  Public helpers used by the editor
  // ---------------------------------------------------------------------
  Entity createShape(const std::string &kind, float x, float y);

  Entity createBackground(float width, float height);

  void attachCppScript(flecs::entity e, std::string path);

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

  // Find an entity by its C++ script path
  flecs::entity findEntityByCppScriptPath(const std::string &path) {
    flecs::entity result;
    world->each<CppScriptComponent>([&](flecs::entity e, CppScriptComponent &s) {
      if (s.source_path == path) {
        result = e;
      }
    });
    return result;
  }

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
