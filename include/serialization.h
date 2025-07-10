#pragma once

#include "ecs.h"
#include "scene.h"
#include <QJsonArray>
#include <QJsonObject>

/**
 * Serialize a single entity (and all its runtime components) into the same
 * JSON structure the editor uses for copy/paste and file save.
 */
inline QJsonObject serializeEntity(const Scene &, Entity e) {
  QJsonObject o;

  // Name ------------------------------------------------------------------
  if (e.has<NameComponent>()) {
    auto &n = e.get<NameComponent>();
    o["NameComponent"] = QString::fromStdString(n.name);
  }

  // Transform -------------------------------------------------------------
  if (e.has<TransformComponent>()) {
    auto &t = e.get<TransformComponent>();
    QJsonObject j;
    j["x"] = t.x;
    j["y"] = t.y;
    j["rotation"] = t.rotation;
    j["sx"] = t.sx;
    j["sy"] = t.sy;
    o["TransformComponent"] = j;
  }

  // Material --------------------------------------------------------------
  if (e.has<MaterialComponent>()) {
    auto &m = e.get<MaterialComponent>();
    QJsonObject j;
    j["color"] = static_cast<qint64>(m.color);
    j["isFilled"] = m.isFilled;
    j["isStroked"] = m.isStroked;
    j["strokeWidth"] = m.strokeWidth;
    j["antiAliased"] = m.antiAliased;
    o["MaterialComponent"] = j;
  }

  // Animation -------------------------------------------------------------
  if (e.has<AnimationComponent>()) {
    auto &a = e.get<AnimationComponent>();
    QJsonObject j;
    j["entryTime"] = a.entryTime;
    j["exitTime"] = a.exitTime;
    o["AnimationComponent"] = j;
  }

  // Script ----------------------------------------------------------------
  if (e.has<ScriptComponent>()) {
    auto &s = e.get<ScriptComponent>();
    QJsonObject j;
    j["scriptPath"] = QString::fromStdString(s.scriptPath);
    j["startFunction"] = QString::fromStdString(s.startFunction);
    j["updateFunction"] = QString::fromStdString(s.updateFunction);
    j["destroyFunction"] = QString::fromStdString(s.destroyFunction);
    o["ScriptComponent"] = j;
  }

  // CppScript -------------------------------------------------------------
  if (e.has<CppScriptComponent>()) {
    auto &s = e.get<CppScriptComponent>();
    QJsonObject j;
    j["source_path"] = QString::fromStdString(s.source_path);
    o["CppScriptComponent"] = j;
  }

  // Path Effect ----------------------------------------------------------
  if (e.has<PathEffectComponent>()) {
    auto &pe = e.get<PathEffectComponent>();
    QJsonObject j;
    j["type"] = static_cast<int>(pe.type);
    QJsonArray dashIntervalsArray;
    for (float val : pe.dashIntervals) {
      dashIntervalsArray.append(val);
    }
    j["dashIntervals"] = dashIntervalsArray;
    j["dashPhase"] = pe.dashPhase;
    j["cornerRadius"] = pe.cornerRadius;
    j["discreteLength"] = pe.discreteLength;
    j["discreteDeviation"] = pe.discreteDeviation;
    o["PathEffectComponent"] = j;
  }

  // Tag -------------------------------------------------------------------
  if (e.has<SceneBackgroundComponent>())
    o["SceneBackgroundComponent"] = true;

  // Shape -----------------------------------------------------------------
  if (e.has<ShapeComponent>()) {
    auto &sh = e.get<ShapeComponent>();
    if (!sh.shape)
      return o; // No shape, nothing to serialize

    QJsonObject j;
    j["kind"] = sh.shape->getKindName();
    j["properties"] = sh.shape->serialize();
    o["ShapeComponent"] = j;
  }
  return o;
}
