#pragma once

#include "ecs.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
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

    // --- Expose Skia types to Lua ---
    lua_.new_usertype<SkPoint>("Point", sol::factories(&SkPoint::Make), "x",
                               &SkPoint::fX, "y", &SkPoint::fY);

    lua_.set_function(
        "Color", [](unsigned char a, unsigned char r, unsigned char g,
                    unsigned char b) { return SkColorSetARGB(a, r, g, b); });

    lua_.new_usertype<SkPaint>(
        "Paint", sol::constructors<SkPaint()>(), "setColor",
        sol::resolve<void(SkColor)>(&SkPaint::setColor), "setStroke",
        &SkPaint::setStroke, "setStrokeWidth", &SkPaint::setStrokeWidth,
        "setAntiAlias", &SkPaint::setAntiAlias, "setStyle",
        sol::overload([](SkPaint &p, bool fill, bool stroke) {
          if (fill && stroke)
            p.setStyle(SkPaint::kStrokeAndFill_Style);
          else if (fill)
            p.setStyle(SkPaint::kFill_Style);
          else if (stroke)
            p.setStyle(SkPaint::kStroke_Style);
        }));

    lua_.new_usertype<SkPath>(
        "Path", sol::constructors<SkPath()>(), "moveTo",
        sol::overload([](SkPath &p, float x, float y) { p.moveTo(x, y); },
                      [](SkPath &p, const SkPoint &pt) { p.moveTo(pt); }),
        "lineTo",
        sol::overload([](SkPath &p, float x, float y) { p.lineTo(x, y); },
                      [](SkPath &p, const SkPoint &pt) { p.lineTo(pt); }),
        "cubicTo",
        sol::overload([](SkPath &p, float x1, float y1, float x2, float y2,
                         float x3,
                         float y3) { p.cubicTo(x1, y1, x2, y2, x3, y3); },
                      [](SkPath &p, const SkPoint &p1, const SkPoint &p2,
                         const SkPoint &p3) { p.cubicTo(p1, p2, p3); }),
        "close", &SkPath::close, "addRect",
        [](SkPath &p, float x, float y, float w, float h) {
          p.addRect(SkRect::MakeXYWH(x, y, w, h));
        },
        "addCircle",
        [](SkPath &p, float cx, float cy, float r) { p.addCircle(cx, cy, r); });

    lua_.new_usertype<SkCanvas>(
        "Canvas", "save", &SkCanvas::save, "restore", &SkCanvas::restore,
        "translate", &SkCanvas::translate, "rotate",
        sol::resolve<void(SkScalar)>(&SkCanvas::rotate), "scale",
        &SkCanvas::scale, "drawLine",
        [](SkCanvas &c, float x0, float y0, float x1, float y1,
           const SkPaint &p) { c.drawLine(x0, y0, x1, y1, p); },
        "drawRect",
        [](SkCanvas &c, float x, float y, float w, float h, const SkPaint &p) {
          c.drawRect(SkRect::MakeXYWH(x, y, w, h), p);
        },
        "drawCircle",
        [](SkCanvas &c, float cx, float cy, float r, const SkPaint &p) {
          c.drawCircle(cx, cy, r, p);
        },
        "drawPath",
        [](SkCanvas &c, const SkPath &path, const SkPaint &p) {
          c.drawPath(path, p);
        });

    // Minimal “registry” proxy (just the world itself)
    auto reg_type = lua_.new_usertype<flecs::world>("Registry");
    reg_type["get_transform"] = [](flecs::world &,
                                   Entity e) -> TransformComponent & {
      return e.get_mut<TransformComponent>();
    };
    reg_type["get_material"] = [](flecs::world &,
                                  Entity e) -> MaterialComponent & {
      return e.get_mut<MaterialComponent>();
    };

    lua_["registry"] = std::ref(world_);
  }

  sol::table loadScript(const std::string &path, Entity e) {
    if (path.empty() || QFileInfo(QString::fromStdString(path)).isDir()) {
      return sol::nil;
    }
    try {
      sol::environment env(lua_, sol::create, lua_.globals());
      env["entity_id"] = e;
      env["registry"] = std::ref(world_);
      env["print"] = lua_["print"];

      QFileInfo fileInfo(QString::fromStdString(path));
      QString scriptPath;
      if (fileInfo.isAbsolute()) {
        scriptPath = fileInfo.absoluteFilePath();
      } else {
        scriptPath = QCoreApplication::applicationDirPath() + "/" +
                     QString::fromStdString(path);
      }

      if (!QFileInfo::exists(scriptPath) || QFileInfo(scriptPath).isDir()) {
        qWarning() << "Script path is invalid or a directory:" << scriptPath;
        return sol::nil;
      }

      lua_.script_file(scriptPath.toStdString(), env);
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

  void call_draw(sol::table &env, const std::string &fn, SkCanvas *canvas) {
    if (env.valid() && env[fn].valid()) {
      sol::protected_function func = env[fn];
      sol::protected_function_result result =
          func(env["entity_id"].get<Entity>(), std::ref(world_), canvas);
      if (!result.valid()) {
        sol::error err = result;
        qWarning() << "Lua error in" << fn.c_str() << ":" << err.what();
      }
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

private:
  flecs::world &world_;
  ScriptingEngine &engine_;
};
