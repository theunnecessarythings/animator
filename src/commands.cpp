// #include "commands.h"
// #include "serialization.h"
// #include "window.h"
//
// //
// //  Utility helpers
// //
// ──────────────────────────────────────────────────────────────────────────────
// static inline QJsonObject snapshotEntity(Scene &scene, Entity e) {
//   return serializeEntity(scene, e);
// }
//
// static inline Entity createFrom(Scene &scene, const QJsonObject &json) {
//   Entity e = scene.reg.create();
//   ComponentRegistry::instance().apply(scene, e, json);
//   return e;
// }
//
// static inline void destroyList(Scene &scene, const QList<Entity> &list) {
//   for (Entity e : list)
//     scene.reg.destroy(e);
// }
//
// static inline QList<Entity> recreateList(Scene &scene,
//                                          const QList<QJsonObject> &jsons) {
//   QList<Entity> out;
//   out.reserve(jsons.size());
//   for (const auto &j : jsons)
//     out.append(createFrom(scene, j));
//   return out;
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  AddEntityCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// AddEntityCommand::AddEntityCommand(MainWindow *w, Entity e, QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e),
//       m_entityData(snapshotEntity(w->canvas()->scene(), e)) {
//   setText(QObject::tr("Add Entity"));
// }
//
// void AddEntityCommand::undo() {
//   m_mainWindow->canvas()->scene().reg.destroy(m_entity);
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// void AddEntityCommand::redo() {
//   static bool first = true;
//   if (first) {
//     first = false;
//     return;
//   } // initial create already done
//
//   m_entity = createFrom(m_mainWindow->canvas()->scene(), m_entityData);
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  RemoveEntityCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// RemoveEntityCommand::RemoveEntityCommand(MainWindow *w, Entity e,
//                                          QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e),
//       m_entityData(snapshotEntity(w->canvas()->scene(), e)) {
//   setText(QObject::tr("Remove Entity"));
// }
//
// void RemoveEntityCommand::redo() {
//   m_mainWindow->canvas()->scene().reg.destroy(m_entity);
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// void RemoveEntityCommand::undo() {
//   // Re-create and update stored id (Flecs may allocate a new one)
//   m_entity = createFrom(m_mainWindow->canvas()->scene(), m_entityData);
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  Cut / Delete multi-selection
// //
// ──────────────────────────────────────────────────────────────────────────────
// CutCommand::CutCommand(MainWindow *w, const QList<Entity> &sel, QUndoCommand
// *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entities(sel) {
//   setText(QObject::tr("Cut Entities"));
//   Scene &scene = w->canvas()->scene();
//   for (Entity e : sel)
//     m_entitiesData.append(snapshotEntity(scene, e));
// }
//
// void CutCommand::redo() {
//   destroyList(m_mainWindow->canvas()->scene(), m_entities);
//   m_entities.clear();
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// void CutCommand::undo() {
//   m_entities = recreateList(m_mainWindow->canvas()->scene(), m_entitiesData);
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// //
// -----------------------------------------------------------------------------
// // DeleteCommand
// //
// -----------------------------------------------------------------------------
// DeleteCommand::DeleteCommand(MainWindow *w, const QList<Entity> &ents,
//                              QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entities(ents) {
//   setText(QObject::tr("Delete Entities"));
//   Scene &scene = w->canvas()->scene();
//   for (Entity e : ents)
//     m_entitiesData.append(snapshotEntity(scene, e));
// }
//
// void DeleteCommand::redo() {
//   destroyList(m_mainWindow->canvas()->scene(), m_entities);
//   m_entities.clear();
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// void DeleteCommand::undo() {
//   m_entities = recreateList(m_mainWindow->canvas()->scene(), m_entitiesData);
//   m_mainWindow->sceneModel()->refresh();
//   m_mainWindow->canvas()->update();
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  MoveEntityCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// MoveEntityCommand::MoveEntityCommand(MainWindow *w, Entity e, float oldX,
//                                      float oldY, float oldR, float newX,
//                                      float newY, float newR, QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldX(oldX),
//     m_oldY(oldY),
//       m_oldRot(oldR), m_newX(newX), m_newY(newY), m_newRot(newR) {
//   setText(QObject::tr("Move Entity"));
// }
//
// void MoveEntityCommand::undo() {
//   if (auto *t = m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
//           m_entity)) {
//     t->x = m_oldX;
//     t->y = m_oldY;
//     t->rotation = m_oldRot;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// void MoveEntityCommand::redo() {
//   if (auto *t = m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
//           m_entity)) {
//     t->x = m_newX;
//     t->y = m_newY;
//     t->rotation = m_newRot;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  ChangeNameCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// ChangeNameCommand::ChangeNameCommand(MainWindow *w, Entity e,
//                                      const std::string &oldN,
//                                      const std::string &newN, QUndoCommand
//                                      *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldName(oldN),
//       m_newName(newN) {
//   setText(QObject::tr("Change Entity Name"));
// }
//
// void ChangeNameCommand::undo() {
//   if (auto *n =
//           m_mainWindow->canvas()->scene().reg.get<NameComponent>(m_entity)) {
//     n->name = m_oldName;
//     m_mainWindow->sceneModel()->refresh();
//     m_mainWindow->canvas()->update();
//   }
// }
//
// void ChangeNameCommand::redo() {
//   if (auto *n =
//           m_mainWindow->canvas()->scene().reg.get<NameComponent>(m_entity)) {
//     n->name = m_newName;
//     m_mainWindow->sceneModel()->refresh();
//     m_mainWindow->canvas()->update();
//   }
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  ChangeTransformCommand  (with merge)
// //
// ──────────────────────────────────────────────────────────────────────────────
// ChangeTransformCommand::ChangeTransformCommand(MainWindow *w, Entity e,
//                                                float oX, float oY, float oR,
//                                                float oSx, float oSy, float
//                                                nX, float nY, float nR, float
//                                                nSx, float nSy, QUndoCommand
//                                                *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldX(oX), m_oldY(oY),
//       m_oldRot(oR), m_oldSx(oSx), m_oldSy(oSy), m_newX(nX), m_newY(nY),
//       m_newRot(nR), m_newSx(nSx), m_newSy(nSy) {
//   setText(QObject::tr("Change Entity Transform"));
// }
//
// void ChangeTransformCommand::undo() {
//   if (auto *t = m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
//           m_entity)) {
//     t->x = m_oldX;
//     t->y = m_oldY;
//     t->rotation = m_oldRot;
//     t->sx = m_oldSx;
//     t->sy = m_oldSy;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// void ChangeTransformCommand::redo() {
//   if (auto *t = m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
//           m_entity)) {
//     t->x = m_newX;
//     t->y = m_newY;
//     t->rotation = m_newRot;
//     t->sx = m_newSx;
//     t->sy = m_newSy;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// bool ChangeTransformCommand::mergeWith(const QUndoCommand *other) {
//   if (other->id() != Id)
//     return false;
//   const auto *o = static_cast<const ChangeTransformCommand *>(other);
//   if (m_entity != o->m_entity)
//     return false;
//
//   m_newX = o->m_newX;
//   m_newY = o->m_newY;
//   m_newRot = o->m_newRot;
//   m_newSx = o->m_newSx;
//   m_newSy = o->m_newSy;
//   return true;
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  ChangeMaterialCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// ChangeMaterialCommand::ChangeMaterialCommand(MainWindow *w, Entity e,
//                                              SkColor oCol, bool oFill,
//                                              bool oStroke, float oSw, bool
//                                              oAA, SkColor nCol, bool nFill,
//                                              bool nStroke, float nSw, bool
//                                              nAA, QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldColor(oCol),
//       m_oldIsFilled(oFill), m_oldIsStroked(oStroke), m_oldStrokeWidth(oSw),
//       m_oldAA(oAA), m_newColor(nCol), m_newIsFilled(nFill),
//       m_newIsStroked(nStroke), m_newStrokeWidth(nSw), m_newAA(nAA) {
//   setText(QObject::tr("Change Entity Material"));
// }
//
// void ChangeMaterialCommand::undo() {
//   if (auto *m = m_mainWindow->canvas()->scene().reg.get<MaterialComponent>(
//           m_entity)) {
//     m->color = m_oldColor;
//     m->isFilled = m_oldIsFilled;
//     m->isStroked = m_oldIsStroked;
//     m->strokeWidth = m_oldStrokeWidth;
//     m->antiAliased = m_oldAA;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// void ChangeMaterialCommand::redo() {
//   if (auto *m = m_mainWindow->canvas()->scene().reg.get<MaterialComponent>(
//           m_entity)) {
//     m->color = m_newColor;
//     m->isFilled = m_newIsFilled;
//     m->isStroked = m_newIsStroked;
//     m->strokeWidth = m_newStrokeWidth;
//     m->antiAliased = m_newAA;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  ChangeAnimationCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// ChangeAnimationCommand::ChangeAnimationCommand(MainWindow *w, Entity e,
//                                                float oEntry, float oExit,
//                                                float nEntry, float nExit,
//                                                QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldEntry(oEntry),
//       m_oldExit(oExit), m_newEntry(nEntry), m_newExit(nExit) {
//   setText(QObject::tr("Change Entity Animation"));
// }
//
// void ChangeAnimationCommand::undo() {
//   if (auto *a = m_mainWindow->canvas()->scene().reg.get<AnimationComponent>(
//           m_entity)) {
//     a->entryTime = m_oldEntry;
//     a->exitTime = m_oldExit;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// void ChangeAnimationCommand::redo() {
//   if (auto *a = m_mainWindow->canvas()->scene().reg.get<AnimationComponent>(
//           m_entity)) {
//     a->entryTime = m_newEntry;
//     a->exitTime = m_newExit;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  ChangeScriptCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// ChangeScriptCommand::ChangeScriptCommand(
//     MainWindow *w, Entity e, const std::string &oPath,
//     const std::string &oStart, const std::string &oUpdate,
//     const std::string &oDestroy, const std::string &nPath,
//     const std::string &nStart, const std::string &nUpdate,
//     const std::string &nDestroy, QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldPath(oPath),
//       m_oldStart(oStart), m_oldUpdate(oUpdate), m_oldDestroy(oDestroy),
//       m_newPath(nPath), m_newStart(nStart), m_newUpdate(nUpdate),
//       m_newDestroy(nDestroy) {
//   setText(QObject::tr("Change Entity Script"));
// }
//
// void ChangeScriptCommand::undo() {
//   if (auto *s =
//           m_mainWindow->canvas()->scene().reg.get<ScriptComponent>(m_entity))
//           {
//     s->scriptPath = m_oldPath;
//     s->startFunction = m_oldStart;
//     s->updateFunction = m_oldUpdate;
//     s->destroyFunction = m_oldDestroy;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// void ChangeScriptCommand::redo() {
//   if (auto *s =
//           m_mainWindow->canvas()->scene().reg.get<ScriptComponent>(m_entity))
//           {
//     s->scriptPath = m_newPath;
//     s->startFunction = m_newStart;
//     s->updateFunction = m_newUpdate;
//     s->destroyFunction = m_newDestroy;
//     m_mainWindow->canvas()->update();
//   }
// }
//
// //
// ──────────────────────────────────────────────────────────────────────────────
// //  ChangeShapePropertyCommand
// //
// ──────────────────────────────────────────────────────────────────────────────
// ChangeShapePropertyCommand::ChangeShapePropertyCommand(MainWindow *w, Entity
// e,
//                                                        const QJsonObject
//                                                        &oldP, const
//                                                        QJsonObject &newP,
//                                                        QUndoCommand *p)
//     : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldProps(oldP),
//       m_newProps(newP) {
//   setText(QObject::tr("Change Shape Property"));
// }
//
// void ChangeShapePropertyCommand::undo() {
//   if (auto *sc =
//           m_mainWindow->canvas()->scene().reg.get<ShapeComponent>(m_entity))
//     if (sc->shape) {
//       sc->shape->deserialize(m_oldProps);
//       m_mainWindow->canvas()->update();
//     }
// }
//
// void ChangeShapePropertyCommand::redo() {
//   if (auto *sc =
//           m_mainWindow->canvas()->scene().reg.get<ShapeComponent>(m_entity))
//     if (sc->shape) {
//       sc->shape->deserialize(m_newProps);
//       m_mainWindow->canvas()->update();
//     }
// }

#include "commands.h"
#include "scene.h"
#include "serialization.h"
#include "window.h"

// ---------------------------------------------------------------------------
//  Local helpers (re‑implemented without ComponentRegistry)
// ---------------------------------------------------------------------------
static inline QJsonObject snapshotEntity(Scene &scene, Entity e) {
  return serializeEntity(scene, e);
}

static inline void applyComponentJson(Scene &scene, Entity e,
                                      const QJsonObject &j);

static inline Entity createFrom(Scene &scene, const QJsonObject &json) {
  Entity e = scene.ecs().entity();
  applyComponentJson(scene, e, json);
  return e;
}

static inline void destroyList(Scene &scene, const QList<Entity> &list) {
  for (Entity e : list)
    e.destruct();
}

static inline QList<Entity> recreateList(Scene &scene,
                                         const QList<QJsonObject> &jsons) {
  QList<Entity> out;
  out.reserve(jsons.size());
  for (const auto &j : jsons)
    out.append(createFrom(scene, j));
  return out;
}

// ---------------------------------------------------------------------------
//  JSON → Components (single entity) helper
// ---------------------------------------------------------------------------
static inline void applyComponentJson(Scene &scene, Entity e,
                                      const QJsonObject &o) {
  // Name ------------------------------------------------------------------
  if (o.contains("NameComponent"))
    e.set<NameComponent>({o["NameComponent"].toString().toStdString()});
  // Transform -------------------------------------------------------------
  if (o.contains("TransformComponent")) {
    const QJsonObject j = o["TransformComponent"].toObject();
    e.set<TransformComponent>({static_cast<float>(j["x"].toDouble()),
                               static_cast<float>(j["y"].toDouble()),
                               static_cast<float>(j["rotation"].toDouble()),
                               static_cast<float>(j["sx"].toDouble()),
                               static_cast<float>(j["sy"].toDouble())});
  }
  // Material --------------------------------------------------------------
  if (o.contains("MaterialComponent")) {
    const QJsonObject j = o["MaterialComponent"].toObject();
    e.set<MaterialComponent>(
        {static_cast<SkColor>(j["color"].toVariant().toULongLong()),
         j["isFilled"].toBool(), j["isStroked"].toBool(),
         static_cast<float>(j["strokeWidth"].toDouble()),
         j["antiAliased"].toBool()});
  }
  // Animation -------------------------------------------------------------
  if (o.contains("AnimationComponent")) {
    const QJsonObject j = o["AnimationComponent"].toObject();
    e.set<AnimationComponent>({static_cast<float>(j["entryTime"].toDouble()),
                               static_cast<float>(j["exitTime"].toDouble())});
  }
  // Script ----------------------------------------------------------------
  if (o.contains("ScriptComponent")) {
    const QJsonObject j = o["ScriptComponent"].toObject();
    e.set<ScriptComponent>({j["scriptPath"].toString().toStdString(),
                            j["startFunction"].toString().toStdString(),
                            j["updateFunction"].toString().toStdString(),
                            j["destroyFunction"].toString().toStdString(),
                            {}});
  }
  // Tag -------------------------------------------------------------------
  if (o.contains("SceneBackgroundComponent"))
    e.add<SceneBackgroundComponent>();
  // Shape -----------------------------------------------------------------
  if (o.contains("ShapeComponent")) {
    const QJsonObject j = o["ShapeComponent"].toObject();
    std::string kind = j["kind"].toString().toStdString();
    auto shp = ShapeFactory::create(kind);
    if (shp && j.contains("properties"))
      shp->deserialize(j["properties"].toObject());
    e.set<ShapeComponent>({std::move(shp)});
  }
}

// ---------------------------------------------------------------------------
//  AddEntityCommand
// ---------------------------------------------------------------------------
AddEntityCommand::AddEntityCommand(MainWindow *w, Entity e, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e),
      m_entityData(snapshotEntity(w->canvas()->scene(), e)) {
  setText(QObject::tr("Add Entity"));
}

void AddEntityCommand::undo() {
  m_entity.destruct();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void AddEntityCommand::redo() {
  if (m_firstRedo) {
    m_firstRedo = false;
    return;
  } // original create already done
  m_entity = createFrom(m_mainWindow->canvas()->scene(), m_entityData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// ---------------------------------------------------------------------------
//  RemoveEntityCommand
// ---------------------------------------------------------------------------
RemoveEntityCommand::RemoveEntityCommand(MainWindow *w, Entity e,
                                         QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e),
      m_entityData(snapshotEntity(w->canvas()->scene(), e)) {
  setText(QObject::tr("Remove Entity"));
}

void RemoveEntityCommand::redo() {
  m_entity.destruct();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void RemoveEntityCommand::undo() {
  m_entity = createFrom(m_mainWindow->canvas()->scene(), m_entityData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// ---------------------------------------------------------------------------
//  Cut / Delete multi‑selection
// ---------------------------------------------------------------------------
CutCommand::CutCommand(MainWindow *w, const QList<Entity> &sel, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entities(sel) {
  setText(QObject::tr("Cut Entities"));
  Scene &scene = w->canvas()->scene();
  for (Entity e : sel)
    m_entitiesData.append(snapshotEntity(scene, e));
}

void CutCommand::redo() {
  destroyList(m_mainWindow->canvas()->scene(), m_entities);
  m_entities.clear();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void CutCommand::undo() {
  m_entities = recreateList(m_mainWindow->canvas()->scene(), m_entitiesData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

DeleteCommand::DeleteCommand(MainWindow *w, const QList<Entity> &ents,
                             QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entities(ents) {
  setText(QObject::tr("Delete Entities"));
  Scene &scene = w->canvas()->scene();
  for (Entity e : ents)
    m_entitiesData.append(snapshotEntity(scene, e));
}

void DeleteCommand::redo() {
  destroyList(m_mainWindow->canvas()->scene(), m_entities);
  m_entities.clear();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void DeleteCommand::undo() {
  m_entities = recreateList(m_mainWindow->canvas()->scene(), m_entitiesData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

MoveEntityCommand::MoveEntityCommand(MainWindow *w, Entity e, float oldX,
                                     float oldY, float oldR, float newX,
                                     float newY, float newR, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldX(oldX), m_oldY(oldY),
      m_oldRot(oldR), m_newX(newX), m_newY(newY), m_newRot(newR) {
  setText(QObject::tr("Move Entity"));
}

void MoveEntityCommand::undo() {
  if (m_entity.has<TransformComponent>()) {
    auto &t = m_entity.get_mut<TransformComponent>();
    t.x = m_oldX;
    t.y = m_oldY;
    t.rotation = m_oldRot;
    m_mainWindow->canvas()->update();
  }
}

void MoveEntityCommand::redo() {
  if (m_entity.has<TransformComponent>()) {
    auto &t = m_entity.get_mut<TransformComponent>();
    t.x = m_newX;
    t.y = m_newY;
    t.rotation = m_newRot;
    m_mainWindow->canvas()->update();
  }
}

ChangeNameCommand::ChangeNameCommand(MainWindow *w, Entity e,
                                     const std::string &oldN,
                                     const std::string &newN, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldName(oldN),
      m_newName(newN) {
  setText(QObject::tr("Change Entity Name"));
}

void ChangeNameCommand::undo() {
  if (m_entity.has<NameComponent>()) {
    auto &n = m_entity.get_mut<NameComponent>();
    n.name = m_oldName;
    m_mainWindow->sceneModel()->refresh();
    m_mainWindow->canvas()->update();
  }
}

void ChangeNameCommand::redo() {
  if (m_entity.has<NameComponent>()) {
    auto &n = m_entity.get_mut<NameComponent>();
    n.name = m_newName;
    m_mainWindow->sceneModel()->refresh();
    m_mainWindow->canvas()->update();
  }
}

ChangeTransformCommand::ChangeTransformCommand(MainWindow *w, Entity e,
                                               float oX, float oY, float oR,
                                               float oSx, float oSy, float nX,
                                               float nY, float nR, float nSx,
                                               float nSy, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldX(oX), m_oldY(oY),
      m_oldRot(oR), m_oldSx(oSx), m_oldSy(oSy), m_newX(nX), m_newY(nY),
      m_newRot(nR), m_newSx(nSx), m_newSy(nSy) {
  setText(QObject::tr("Change Entity Transform"));
}

void ChangeTransformCommand::undo() {
  if (m_entity.has<TransformComponent>()) {
    auto &t = m_entity.get_mut<TransformComponent>();
    t.x = m_oldX;
    t.y = m_oldY;
    t.rotation = m_oldRot;
    t.sx = m_oldSx;
    t.sy = m_oldSy;
    m_mainWindow->canvas()->update();
  }
}

void ChangeTransformCommand::redo() {
  if (m_entity.has<TransformComponent>()) {
    auto &t = m_entity.get_mut<TransformComponent>();
    t.x = m_newX;
    t.y = m_newY;
    t.rotation = m_newRot;
    t.sx = m_newSx;
    t.sy = m_newSy;
    m_mainWindow->canvas()->update();
  }
}

bool ChangeTransformCommand::mergeWith(const QUndoCommand *other) {
  if (other->id() != Id)
    return false;
  const auto *o = static_cast<const ChangeTransformCommand *>(other);
  if (m_entity != o->m_entity)
    return false;

  m_newX = o->m_newX;
  m_newY = o->m_newY;
  m_newRot = o->m_newRot;
  m_newSx = o->m_newSx;
  m_newSy = o->m_newSy;
  return true;
}

ChangeMaterialCommand::ChangeMaterialCommand(MainWindow *w, Entity e,
                                             SkColor oCol, bool oFill,
                                             bool oStroke, float oSw, bool oAA,
                                             SkColor nCol, bool nFill,
                                             bool nStroke, float nSw, bool nAA,
                                             QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldColor(oCol),
      m_oldFill(oFill), m_oldStroke(oStroke), m_oldWidth(oSw), m_oldAA(oAA),
      m_newColor(nCol), m_newFill(nFill), m_newStroke(nStroke), m_newWidth(nSw),
      m_newAA(nAA) {
  setText(QObject::tr("Change Entity Material"));
}

void ChangeMaterialCommand::undo() {
  if (m_entity.has<MaterialComponent>()) {
    auto &m = m_entity.get_mut<MaterialComponent>();
    m.color = m_oldColor;
    m.isFilled = m_oldFill;
    m.isStroked = m_oldStroke;
    m.strokeWidth = m_oldWidth;
    m.antiAliased = m_oldAA;
    m_mainWindow->canvas()->update();
  }
}

void ChangeMaterialCommand::redo() {
  if (m_entity.has<MaterialComponent>()) {
    auto &m = m_entity.get_mut<MaterialComponent>();
    m.color = m_newColor;
    m.isFilled = m_newFill;
    m.isStroked = m_newStroke;
    m.strokeWidth = m_newWidth;
    m.antiAliased = m_newAA;
    m_mainWindow->canvas()->update();
  }
}

ChangeAnimationCommand::ChangeAnimationCommand(MainWindow *w, Entity e,
                                               float oEntry, float oExit,
                                               float nEntry, float nExit,
                                               QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldEntry(oEntry),
      m_oldExit(oExit), m_newEntry(nEntry), m_newExit(nExit) {
  setText(QObject::tr("Change Entity Animation"));
}

void ChangeAnimationCommand::undo() {
  if (m_entity.has<AnimationComponent>()) {
    auto &a = m_entity.get_mut<AnimationComponent>();
    a.entryTime = m_oldEntry;
    a.exitTime = m_oldExit;
    m_mainWindow->canvas()->update();
  }
}

void ChangeAnimationCommand::redo() {
  if (m_entity.has<AnimationComponent>()) {
    auto &a = m_entity.get_mut<AnimationComponent>();
    a.entryTime = m_newEntry;
    a.exitTime = m_newExit;
    m_mainWindow->canvas()->update();
  }
}

ChangeScriptCommand::ChangeScriptCommand(
    MainWindow *w, Entity e, const std::string &oPath,
    const std::string &oStart, const std::string &oUpdate,
    const std::string &oDestroy, const std::string &nPath,
    const std::string &nStart, const std::string &nUpdate,
    const std::string &nDestroy, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldPath(oPath),
      m_oldStart(oStart), m_oldUpdate(oUpdate), m_oldDestroy(oDestroy),
      m_newPath(nPath), m_newStart(nStart), m_newUpdate(nUpdate),
      m_newDestroy(nDestroy) {
  setText(QObject::tr("Change Entity Script"));
}

void ChangeScriptCommand::undo() {
  if (m_entity.has<ScriptComponent>()) {
    auto &s = m_entity.get_mut<ScriptComponent>();
    s.scriptPath = m_oldPath;
    s.startFunction = m_oldStart;
    s.updateFunction = m_oldUpdate;
    s.destroyFunction = m_oldDestroy;
    m_mainWindow->canvas()->update();
  }
}

void ChangeScriptCommand::redo() {
  if (m_entity.has<ScriptComponent>()) {
    auto &s = m_entity.get_mut<ScriptComponent>();
    s.scriptPath = m_newPath;
    s.startFunction = m_newStart;
    s.updateFunction = m_newUpdate;
    s.destroyFunction = m_newDestroy;
    m_mainWindow->canvas()->update();
  }
}

ChangeShapePropertyCommand::ChangeShapePropertyCommand(MainWindow *w, Entity e,
                                                       const QJsonObject &oldP,
                                                       const QJsonObject &newP,
                                                       QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e), m_oldProps(oldP),
      m_newProps(newP) {
  setText(QObject::tr("Change Shape Property"));
}

void ChangeShapePropertyCommand::undo() {
  if (m_entity.has<ShapeComponent>()) {
    auto &sc = m_entity.get_mut<ShapeComponent>();
    if (sc.shape) {
      sc.shape->deserialize(m_oldProps);
      m_mainWindow->canvas()->update();
    }
  }
}

void ChangeShapePropertyCommand::redo() {
  if (m_entity.has<ShapeComponent>()) {
    auto &sc = m_entity.get_mut<ShapeComponent>();
    if (sc.shape) {
      sc.shape->deserialize(m_newProps);
      m_mainWindow->canvas()->update();
    }
  }
}
