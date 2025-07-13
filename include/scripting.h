#pragma once

#include "ecs.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QString>
#include <sol/sol.hpp>

class SkiaCanvasWidget;

// ----------------------------------------------------------------------------
//  ScriptingEngine – owns the Lua state, loads scripts
// ----------------------------------------------------------------------------
class ScriptingEngine {
public:
  explicit ScriptingEngine(flecs::world &w, SkiaCanvasWidget *canvas);
  sol::table loadScript(const std::string &path, Entity e);
  void call(sol::table &env, const std::string &fn, float dt = 0.f,
            float t = 0.f);
  void call_draw(sol::table &env, const std::string &fn, SkCanvas *canvas);

private:
  sol::state lua_;
  flecs::world &world_;
  SkiaCanvasWidget *canvas_;
};

// ----------------------------------------------------------------------------
//  ScriptSystem
// ----------------------------------------------------------------------------
class ScriptSystem {
public:
  ScriptSystem(flecs::world &w, ScriptingEngine &se) : world_(w), engine_(se) {}

  ScriptingEngine &getEngine() { return engine_; }

  void resetEnvironments() {
    world_.each<ScriptComponent>([this](flecs::entity, ScriptComponent &sc) {
      if (sc.scriptEnv.valid()) {
        engine_.call(sc.scriptEnv, sc.destroyFunction);
      }
      sc.scriptEnv = sol::nil;
    });
  }

  void tick(float dt, float currentTime) {
    world_.each<ScriptComponent>([&](flecs::entity e, ScriptComponent &sc) {
      // Lazy-load script
      if (!sc.scriptEnv.valid()) {
        qDebug() << "Script environment not valid for entity" << e.id()
                 << ", loading script:" << sc.scriptPath.c_str();
        sc.scriptEnv = engine_.loadScript(sc.scriptPath, e);
        if (sc.scriptEnv.valid()) {
          engine_.call(sc.scriptEnv, sc.startFunction);
        } else {
          qWarning() << "Failed to load script for entity" << e.id() << ":"
                     << sc.scriptPath.c_str();
        }
      }

      if (sc.scriptEnv.valid()) {
        if (sc.scriptEnv[sc.updateFunction].valid()) {
          engine_.call(sc.scriptEnv, sc.updateFunction, dt, currentTime);
        } else {
          qWarning() << "Update function '" << sc.updateFunction.c_str()
                     << "' not found in script for entity" << e.id();
        }
      } else {
        qWarning() << "Script environment invalid for update call for entity"
                   << e.id();
      }
    });
  }

  void reloadScript(const std::string &changedPath) {
    world_.each<ScriptComponent>([&](flecs::entity e, ScriptComponent &sc) {
      if (sc.scriptPath == changedPath) {
        if (sc.scriptEnv.valid()) {
          engine_.call(sc.scriptEnv, sc.destroyFunction);
        }
        sc.scriptEnv = sol::nil;
        sc.scriptEnv = engine_.loadScript(sc.scriptPath, e);
        if (sc.scriptEnv.valid()) {
          engine_.call(sc.scriptEnv, sc.startFunction);
        }
      }
    });
  }

private:
  flecs::world &world_;
  ScriptingEngine &engine_;
};
