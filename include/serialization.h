#pragma once
#include "ecs.h"
#include <QJsonArray>
#include <QJsonObject>

/**
 *  Serialise one entity into a QJsonObject that matches the clipboard format
 *  used by onCopy()/onPaste().
 *
 *  Usage:
 *      QJsonArray arr;
 *      for (Entity e : selected)
 *          arr.append(serializeEntity(scene, e));
 *      QApplication::clipboard()->setText(QJsonDocument(arr)
 *                                         .toJson(QJsonDocument::Compact));
 */
inline QJsonObject serializeEntity(const Scene &scene, Entity e) {
  QJsonObject entityObj;

  // ---------- NameComponent ------------------------------------------------
  if (auto *name = scene.reg.get<NameComponent>(e))
    entityObj["NameComponent"] = QString::fromStdString(name->name);

  // ---------- TransformComponent ------------------------------------------
  if (auto *tr = scene.reg.get<TransformComponent>(e)) {
    QJsonObject t;
    t["x"] = tr->x;
    t["y"] = tr->y;
    t["rotation"] = tr->rotation;
    t["sx"] = tr->sx;
    t["sy"] = tr->sy;
    entityObj["TransformComponent"] = t;
  }

  // ---------- MaterialComponent -------------------------------------------
  if (auto *m = scene.reg.get<MaterialComponent>(e)) {
    QJsonObject mat;
    mat["color"] = static_cast<qint64>(m->color);
    mat["isFilled"] = m->isFilled;
    mat["isStroked"] = m->isStroked;
    mat["strokeWidth"] = m->strokeWidth;
    mat["antiAliased"] = m->antiAliased;
    entityObj["MaterialComponent"] = mat;
  }

  // ---------- AnimationComponent ------------------------------------------
  if (auto *a = scene.reg.get<AnimationComponent>(e)) {
    QJsonObject anim;
    anim["entryTime"] = a->entryTime;
    anim["exitTime"] = a->exitTime;
    entityObj["AnimationComponent"] = anim;
  }

  // ---------- ScriptComponent ---------------------------------------------
  if (auto *s = scene.reg.get<ScriptComponent>(e)) {
    QJsonObject scr;
    scr["scriptPath"] = QString::fromStdString(s->scriptPath);
    scr["startFunction"] = QString::fromStdString(s->startFunction);
    scr["updateFunction"] = QString::fromStdString(s->updateFunction);
    scr["destroyFunction"] = QString::fromStdString(s->destroyFunction);
    entityObj["ScriptComponent"] = scr;
  }

  // ---------- SceneBackgroundComponent (tag) ------------------------------
  if (scene.reg.has<SceneBackgroundComponent>(e))
    entityObj["SceneBackgroundComponent"] = true;

  // ---------- ShapeComponent ----------------------------------------------
  if (auto *sh = scene.reg.get<ShapeComponent>(e)) {
    if (sh->shape) {
      QJsonObject shapeObject;
      shapeObject["kind"] = sh->shape->getKindName();
      shapeObject["properties"] = sh->shape->serialize();
      entityObj["ShapeComponent"] = shapeObject;
    }
  }
  return entityObj;
}
