#pragma once

#include "ecs.h"
#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <sol/sol.hpp>

// ----------------------------------------------------------------------------
//  ScriptingEngine – owns the Lua state, loads scripts
// ----------------------------------------------------------------------------
class ScriptingEngine {
public:
  explicit ScriptingEngine(flecs::world &w) : lua_(), world_(w) {
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

    // Expose C++ components to Lua
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

    // Minimal “registry” proxy (just the world itself)
    auto reg_type = lua_.new_usertype<flecs::world>("Registry");
    reg_type["get_transform"] = [](flecs::world &self, Entity e) {
      return e.get_mut<TransformComponent>();
    };
    reg_type["get_material"] = [](flecs::world &self, Entity e) {
      return e.get_mut<MaterialComponent>();
    };

    lua_["registry"] = std::ref(world_);
  }

  sol::table loadScript(const std::string &path, Entity e) {
    try {
      sol::environment env(lua_, sol::create, lua_.globals());
      env["entity_id"] = e;
      env["registry"] = std::ref(world_);

      QString abs = QCoreApplication::applicationDirPath() + "/" +
                    QString::fromStdString(path);
      lua_.script_file(abs.toStdString(), env);
      return env;
    } catch (const sol::error &er) {
      qWarning() << "Lua load error:" << er.what();
      return sol::nil;
    }
  }

  void call(sol::table &env, const std::string &fn, float dt = 0.f,
            float t = 0.f) {
    if (env.valid() && env[fn].valid()) {
      try {
        env[fn](env["entity_id"].get<Entity>(), std::ref(world_), dt, t);
      } catch (const sol::error &er) {
        qWarning() << "Lua error in" << fn.c_str() << ":" << er.what();
      }
    }
  }

private:
  sol::state lua_;
  flecs::world &world_;
};

// ----------------------------------------------------------------------------
//  ScriptSystem – call this from your main loop
// ----------------------------------------------------------------------------
class ScriptSystem {
public:
  ScriptSystem(flecs::world &w, ScriptingEngine &se) : world_(w), engine_(se) {}

  void tick(float dt, float currentTime) {
    world_.each<ScriptComponent>([&](flecs::entity e, ScriptComponent &sc) {
      // Lazy-load script
      if (!sc.scriptEnv.valid()) {
        sc.scriptEnv = engine_.loadScript(sc.scriptPath, e);
        engine_.call(sc.scriptEnv, sc.startFunction);
      }
      engine_.call(sc.scriptEnv, sc.updateFunction, dt, currentTime);
    });
  }

private:
  flecs::world &world_;
  ScriptingEngine &engine_;
};
