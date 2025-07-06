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

    // Redirect Lua's print to qDebug
    lua_.set_function("print", [](sol::variadic_args args, sol::this_state s) {
      sol::state_view lua(s);
      QStringList messages;
      for (auto a : args) {
        messages << QString::fromStdString(a.as<std::string>());
      }
    });

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
    reg_type["get_transform"] = [](flecs::world &self,
                                   Entity e) -> TransformComponent & {
      return e.get_mut<TransformComponent>();
    };
    reg_type["get_material"] = [](flecs::world &self,
                                  Entity e) -> MaterialComponent & {
      return e.get_mut<MaterialComponent>();
    };

    lua_["registry"] = std::ref(world_);
  }

  sol::table loadScript(const std::string &path, Entity e) {
    try {
      sol::environment env(lua_, sol::create, lua_.globals());
      env["entity_id"] = e;
      env["registry"] = std::ref(world_);
      env["print"] = lua_["print"];

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
      sol::protected_function func = env[fn];
      sol::protected_function_result result =
          func(env["entity_id"].get<Entity>(), std::ref(world_), dt, t);
      if (!result.valid()) {
        sol::error err = result;
        qWarning() << "Lua error in" << fn.c_str() << ":" << err.what();
      }
    } else {
      qWarning() << "Lua function" << fn.c_str() << "not found in script";
    }
  }

private:
  sol::state lua_;
  flecs::world &world_;
};

// ----------------------------------------------------------------------------
//  ScriptSystem
// ----------------------------------------------------------------------------
class ScriptSystem {
public:
  ScriptSystem(flecs::world &w, ScriptingEngine &se) : world_(w), engine_(se) {
    world_.observer<ScriptComponent>()
        .event(flecs::OnRemove)
        .each([this](flecs::entity e, ScriptComponent &sc) {
          if (sc.scriptEnv.valid()) {
            this->engine_.call(sc.scriptEnv, sc.destroyFunction);
            sc.scriptEnv = sol::nil; // Invalidate the Lua table
          }
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

private:
  flecs::world &world_;
  ScriptingEngine &engine_;
};
