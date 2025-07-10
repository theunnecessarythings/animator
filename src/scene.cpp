#include "scene.h"
#include "ecs.h"

#include <QtConcurrent/QtConcurrent>
#include <cstdlib> // for system()
#include <dlfcn.h> // for dlopen, dlsym, dlclose
#include <future>
#include <iostream>
#include <sys/stat.h> // for stat to check file modification times

Scene::Scene()
    : world(std::make_unique<flecs::world>()), scriptingEngine(*world),
      scriptSystem(*world, scriptingEngine), renderer(*world, scriptSystem) {
  world->set<TimeSingleton>({0.f});

  // --- Precompile C++ Script Header ---
  std::cout << "Checking for C++ script precompiled header..." << std::endl;
  std::string pch_source = "../include/script_pch.h";
  std::string pch_output = "../include/script_pch.h.gch";

  struct stat stat_source_buf, stat_output_buf;
  bool needs_recompile = stat(pch_source.c_str(), &stat_source_buf) != 0 ||
                         stat(pch_output.c_str(), &stat_output_buf) != 0 ||
                         stat_output_buf.st_mtime < stat_source_buf.st_mtime;

  std::string includes =
      " -I. -Wall -Wextra -D_REENTRANT -fPIC -DQT_OPENGL_LIB "
      "-DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../../animator -I. "
      "-I/home/sreeraj/ubuntu/Documents/skia -I../sol2/include "
      "-I../lua-5.4.8/src -I../include -I../flecs "
      "-I/usr/include/x86_64-linux-gnu/qt5 "
      "-I/usr/include/x86_64-linux-gnu/qt5/QtOpenGL "
      "-I/usr/include/x86_64-linux-gnu/qt5/QtWidgets "
      "-I/usr/include/x86_64-linux-gnu/qt5/QtGui "
      "-I/usr/include/x86_64-linux-gnu/qt5/QtCore -I. "
      "-I/usr/lib/x86_64-linux-gnu/qt5/mkspecs/linux-g+";

  if (!needs_recompile) {
    std::cout << "PCH is up to date." << std::endl;
  } else {
    std::string pch_command =
        "g++ -x c++-header -o " + pch_output + " " + pch_source + includes;

    // Run compilation asynchronously to not block the main thread
    QtConcurrent::run([pch_command]() {
      std::cout << "PCH generation started in background..." << std::endl;
      int ret = system(pch_command.c_str());
      if (ret != 0) {
        std::cerr << "PCH Error: Failed to compile precompiled header in "
                     "background."
                  << std::endl;
      } else {
        std::cout << "PCH generated successfully in background." << std::endl;
      }
    });
  }

  // ------------------- C++ SCRIPTING SYSTEMS -------------------
  world->observer<CppScriptComponent>()
      .event(flecs::OnSet)
      .each([this, includes](flecs::entity e, CppScriptComponent &script) {
        if (script.source_path.empty())
          return;
        QFileInfo sourceInfo(QString::fromStdString(script.source_path));
        QString baseName = sourceInfo.baseName();
        QString libPath = QCoreApplication::applicationDirPath() + "/plugins/" +
                          baseName + ".so";
        script.library_path = libPath.toStdString();
        qDebug() << "Attempting to load C++ script from:" << libPath;
        std::string command =
            "g++ -O0 -shared -fPIC -include ../include/script_pch.h -o " +
            script.library_path + " " + script.source_path + " -I. " + includes;

        std::cout << "Compiling C++ script: " << command << std::endl;
        int compile_ret = system(command.c_str());
        if (compile_ret != 0) {
          std::cerr << "CppScript Error: Compilation failed for "
                    << script.source_path << std::endl;
          e.remove<CppScriptComponent>();
          return;
        }

        void *handle =
            dlopen(script.library_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
          std::cerr << "CppScript Error: " << dlerror() << std::endl;
          e.remove<CppScriptComponent>();
          return;
        }
        script.library_handle = handle;

        auto create_fn = (IScript * (*)()) dlsym(handle, "create_script");
        if (!create_fn) {
          std::cerr << "CppScript Error: Cannot find create_script."
                    << std::endl;
          dlclose(handle);
          e.remove<CppScriptComponent>();
          return;
        }

        script.script_instance = create_fn();
        script.script_instance->on_start(e, *world);
        std::cout << "Successfully loaded C++ script: " << script.source_path
                  << std::endl;
      });

  world->observer<CppScriptComponent>()
      .event(flecs::OnRemove)
      .each([](flecs::entity, CppScriptComponent &script) {
        if (!script.library_handle)
          return;
        auto destroy_fn =
            (void (*)(IScript *))dlsym(script.library_handle, "destroy_script");
        if (destroy_fn && script.script_instance) {
          destroy_fn(script.script_instance);
        }
        dlclose(script.library_handle);
      });

  world->system<CppScriptComponent>().each(
      [this](flecs::entity e, CppScriptComponent &script) {
        if (script.script_instance) {
          const auto time = world->get<TimeSingleton>();
          script.script_instance->on_update(e, *world, world->delta_time(),
                                            time.time);
        }
      });
}
Entity Scene::createShape(const std::string &kind, float x, float y) {
  Entity e = world->entity();

  e.set<TransformComponent>({x, y});
  e.set<ShapeComponent>(ShapeFactory::create(kind));
  e.set<MaterialComponent>(
      {SkColorSetARGB(255, rand() % 256, rand() % 256, rand() % 256), true,
       false, 1.f, true});

  std::string name = uniqueName(kind);
  e.set<NameComponent>({name});
  return e;
}

Entity Scene::createBackground(float width, float height) {
  Entity e = world->entity("Background");
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

void Scene::attachCppScript(flecs::entity e, std::string path) {
  e.set<CppScriptComponent>({path});
}

// ---------------------------------------------------------------------
//  Frame tick helpers
// ---------------------------------------------------------------------
void Scene::update(float dt, float timelineSeconds) {
  world->get_mut<TimeSingleton>().time = timelineSeconds;
  world->progress(dt);
}

void Scene::draw(SkCanvas *canvas, float timelineSeconds) {
  // The main renderer handles shapes, materials, and probably Lua script
  // drawing.
  renderer.render(canvas, timelineSeconds);

  // We explicitly iterate and draw for C++ scripts here.
  world->each([&](flecs::entity e, CppScriptComponent &script) {
    if (script.script_instance) {
      canvas->save();
      if (e.has<TransformComponent>()) {
        auto &tc = e.get<TransformComponent>();
        canvas->translate(tc.x, tc.y);
        canvas->rotate(tc.rotation * 180.0f / 3.14159265359f);
        canvas->scale(tc.sx, tc.sy);
      }
      script.script_instance->on_draw(e, *world, canvas);
      canvas->restore();
    }
  });
}

// ---------------------------------------------------------------------
//  Serialization helpers
// ---------------------------------------------------------------------
QJsonObject Scene::serialize() const {
  QJsonObject obj;
  QJsonArray arr;
  world->each<const TransformComponent>(
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
          j["drawFunction"] = QString::fromStdString(s.drawFunction);
          j["destroyFunction"] = QString::fromStdString(s.destroyFunction);
          ent["ScriptComponent"] = j;
        }
        if (e.has<CppScriptComponent>()) {
          auto &s = e.get<CppScriptComponent>();
          QJsonObject j;
          j["source_path"] = QString::fromStdString(s.source_path);
          j["library_path"] = QString::fromStdString(s.library_path);
          ent["CppScriptComponent"] = j;
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

void Scene::deserialize(const QJsonObject &root) {
  // clear();
  if (!root.contains("entities") || !root["entities"].isArray())
    return;
  clear();
  const QJsonArray arr = root["entities"].toArray();
  qDebug() << "Deserializing" << arr.size() << "entities.";

  for (const auto &v : arr)
    if (v.isObject()) {
      const QJsonObject eobj = v.toObject();
      Entity e = world->entity();

      // Name -------------------------------------------------------------
      if (eobj.contains("NameComponent"))
        e.set<NameComponent>({eobj["NameComponent"].toString().toStdString()});

      // Transform --------------------------------------------------------
      if (eobj.contains("TransformComponent")) {
        const QJsonObject j = eobj["TransformComponent"].toObject();
        e.set<TransformComponent>({static_cast<float>(j["x"].toDouble()),
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
                                j["drawFunction"].toString().toStdString(),
                                {}}); // env filled on OnAdd
      }

      // C++ Script -------------------------------------------------------
      if (eobj.contains("CppScriptComponent")) {
        const QJsonObject j = eobj["CppScriptComponent"].toObject();
        e.set<CppScriptComponent>({j["source_path"].toString().toStdString()});
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

void Scene::clear() {
  world->delete_with<NameComponent>();
  kindCounters.clear();
}
