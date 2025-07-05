#pragma once
//
//  serializeEntity()  →  QJsonObject
//  Converts a single entity into the clipboard-friendly JSON format used
//  by copy/paste.
//
#include "ecs.h"
#include <QJsonArray>
#include <QJsonObject>

inline QJsonObject serializeEntity(const Scene &scene, Entity e) {
  QJsonObject o;

  // NameComponent ---------------------------------------------------------
  if (auto *n = scene.reg.get<NameComponent>(e))
    o["NameComponent"] = QString::fromStdString(n->name);

  // TransformComponent ----------------------------------------------------
  if (auto *t = scene.reg.get<TransformComponent>(e)) {
    QJsonObject j;
    j["x"] = t->x;
    j["y"] = t->y;
    j["rotation"] = t->rotation;
    j["sx"] = t->sx;
    j["sy"] = t->sy;
    o["TransformComponent"] = j;
  }

  // MaterialComponent -----------------------------------------------------
  if (auto *m = scene.reg.get<MaterialComponent>(e)) {
    QJsonObject j;
    j["color"] = static_cast<qint64>(m->color);
    j["isFilled"] = m->isFilled;
    j["isStroked"] = m->isStroked;
    j["strokeWidth"] = m->strokeWidth;
    j["antiAliased"] = m->antiAliased;
    o["MaterialComponent"] = j;
  }

  // AnimationComponent ----------------------------------------------------
  if (auto *a = scene.reg.get<AnimationComponent>(e)) {
    QJsonObject j;
    j["entryTime"] = a->entryTime;
    j["exitTime"] = a->exitTime;
    o["AnimationComponent"] = j;
  }

  // ScriptComponent -------------------------------------------------------
  if (auto *s = scene.reg.get<ScriptComponent>(e)) {
    QJsonObject j;
    j["scriptPath"] = QString::fromStdString(s->scriptPath);
    j["startFunction"] = QString::fromStdString(s->startFunction);
    j["updateFunction"] = QString::fromStdString(s->updateFunction);
    j["destroyFunction"] = QString::fromStdString(s->destroyFunction);
    o["ScriptComponent"] = j;
  }

  // SceneBackgroundComponent (tag) ---------------------------------------
  if (scene.reg.has<SceneBackgroundComponent>(e))
    o["SceneBackgroundComponent"] = true;

  // ShapeComponent --------------------------------------------------------
  if (auto *sh = scene.reg.get<ShapeComponent>(e); sh && sh->shape) {
    QJsonObject j;
    j["kind"] = sh->shape->getKindName();
    j["properties"] = sh->shape->serialize();
    o["ShapeComponent"] = j;
  }
  return o;
}
