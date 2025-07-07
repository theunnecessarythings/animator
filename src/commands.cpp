#include "commands.h"
#include "scene.h"
#include "serialization.h"
#include "window.h"

// =========================================================================
// HELPERS
// =========================================================================

template <>
const char *SceneCommand::getComponentJsonKey<AnimationComponent>() {
  return "AnimationComponent";
}
template <> const char *SceneCommand::getComponentJsonKey<ScriptComponent>() {
  return "ScriptComponent";
}
template <>
const char *SceneCommand::getComponentJsonKey<PathEffectComponent>() {
  return "PathEffectComponent";
}

static inline QJsonObject snapshotEntity(Scene &scene, Entity e) {
  return serializeEntity(scene, e);
}

void applyJsonToEntity(flecs::world &world, flecs::entity e,
                       const QJsonObject &o, bool is_paste) {
  // Name Component
  if (o.contains("NameComponent")) {
    std::string baseName = o["NameComponent"].toString().toStdString();
    std::string uniqueName = baseName;
    int counter = 1;
    bool is_unique = false;

    while (!is_unique) {
      is_unique = true;
      world.each<const NameComponent>(
          [&](flecs::entity other_e, const NameComponent &nc) {
            if (other_e != e && nc.name == uniqueName) {
              is_unique = false;
            }
          });

      if (!is_unique) {
        uniqueName = baseName + "." + std::to_string(counter++);
      }
    }

    e.set<NameComponent>({uniqueName});
    e.set_name(uniqueName.c_str());
  }

  if (o.contains("TransformComponent")) {
    const QJsonObject j = o["TransformComponent"].toObject();
    float offset = is_paste ? 15.0f : 0.0f;
    e.set<TransformComponent>({static_cast<float>(j["x"].toDouble()) + offset,
                               static_cast<float>(j["y"].toDouble()) + offset,
                               static_cast<float>(j["rotation"].toDouble()),
                               static_cast<float>(j["sx"].toDouble()),
                               static_cast<float>(j["sy"].toDouble())});
  }
  if (o.contains("MaterialComponent")) {
    const QJsonObject j = o["MaterialComponent"].toObject();
    e.set<MaterialComponent>(
        {static_cast<SkColor>(j["color"].toVariant().toULongLong()),
         j["isFilled"].toBool(), j["isStroked"].toBool(),
         static_cast<float>(j["strokeWidth"].toDouble()),
         j["antiAliased"].toBool()});
  }
  if (o.contains("AnimationComponent")) {
    const QJsonObject j = o["AnimationComponent"].toObject();
    e.set<AnimationComponent>({static_cast<float>(j["entryTime"].toDouble()),
                               static_cast<float>(j["exitTime"].toDouble())});
  }
  if (o.contains("ScriptComponent")) {
    const QJsonObject j = o["ScriptComponent"].toObject();
    e.set<ScriptComponent>({j["scriptPath"].toString().toStdString(),
                            j["startFunction"].toString().toStdString(),
                            j["updateFunction"].toString().toStdString(),
                            j["destroyFunction"].toString().toStdString(),
                            {}});
  }
  if (o.contains("PathEffectComponent")) {
    const QJsonObject peJson = o["PathEffectComponent"].toObject();
    PathEffectComponent pe;
    pe.type = static_cast<PathEffectComponent::Type>(peJson["type"].toInt());
    QJsonArray dashIntervalsArray = peJson["dashIntervals"].toArray();
    pe.dashIntervals.clear();
    for (const QJsonValue &val : dashIntervalsArray) {
      pe.dashIntervals.push_back(val.toDouble());
    }
    pe.dashPhase = peJson["dashPhase"].toDouble();
    pe.cornerRadius = peJson["cornerRadius"].toDouble();
    pe.discreteLength = peJson["discreteLength"].toDouble();
    pe.discreteDeviation = peJson["discreteDeviation"].toDouble();
    e.set<PathEffectComponent>(pe);
  }
  if (o.contains("SceneBackgroundComponent"))
    e.add<SceneBackgroundComponent>();
  if (o.contains("ShapeComponent")) {
    const QJsonObject j = o["ShapeComponent"].toObject();
    std::string kind = j["kind"].toString().toStdString();
    auto shp = ShapeFactory::create(kind);
    if (shp && j.contains("properties"))
      shp->deserialize(j["properties"].toObject());
    e.set<ShapeComponent>({std::move(shp)});
  }
}

static inline Entity createFrom(Scene &scene, const QJsonObject &json,
                                QMap<qint64, Entity> &idMap) {
  Entity newEntity = scene.ecs().entity();
  applyJsonToEntity(scene.ecs(), newEntity, json, false);
  if (json.contains("id")) {
    qint64 oldId = json["id"].toInt();
    idMap[oldId] = newEntity;
  }
  return newEntity;
}

static inline QList<Entity> recreateList(MainWindow *mainWindow,
                                         const QList<QJsonObject> &jsons) {
  QList<Entity> newEntities;
  QMap<qint64, Entity> idMap;

  for (const auto &j : jsons) {
    newEntities.append(createFrom(mainWindow->canvas()->scene(), j, idMap));
  }
  QUndoStack *stack = mainWindow->undoStack();
  for (int i = 0; i < stack->count(); ++i) {
    if (auto *cmd = dynamic_cast<SceneCommand *>(
            const_cast<QUndoCommand *>(stack->command(i)))) {
      cmd->updateEntityIds(idMap);
    }
  }

  return newEntities;
}

static inline void destroyList(const QList<Entity> &list) {
  for (Entity e : list) {
    if (e.is_alive()) {
      e.destruct();
    }
  }
}

// =========================================================================
// COMMAND IMPLEMENTATIONS
// =========================================================================

// AddEntityCommand
AddEntityCommand::AddEntityCommand(MainWindow *w, Entity e, QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e) {
  setText(QObject::tr("Add Entity"));
  m_entityData = snapshotEntity(w->canvas()->scene(), e);
  // Store the ID in the JSON snapshot for future lookups
  m_entityData["id"] = (qint64)e.id();
}

void AddEntityCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id())) {
    m_entity = idMap[m_entity.id()];
  }
}

void AddEntityCommand::undo() {
  if (m_entity.is_alive()) {
    m_entity.destruct();
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void AddEntityCommand::redo() {
  if (m_firstRedo) {
    m_firstRedo = false;
    return;
  }
  QMap<qint64, Entity> idMap;
  m_entity = createFrom(m_mainWindow->canvas()->scene(), m_entityData, idMap);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// RemoveEntityCommand
RemoveEntityCommand::RemoveEntityCommand(MainWindow *w, Entity e,
                                         QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e) {
  setText(QObject::tr("Remove Entity"));
  m_entityData = snapshotEntity(w->canvas()->scene(), e);
  m_entityData["id"] = (qint64)e.id();
}

void RemoveEntityCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id())) {
    m_entity = idMap[m_entity.id()];
  }
}

void RemoveEntityCommand::redo() {
  if (m_entity.is_alive()) {
    m_entity.destruct();
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void RemoveEntityCommand::undo() {
  QMap<qint64, Entity> idMap;
  m_entity = createFrom(m_mainWindow->canvas()->scene(), m_entityData, idMap);

  // Update the stack after recreating
  QUndoStack *stack = m_mainWindow->undoStack();
  for (int i = 0; i < stack->count(); ++i) {
    if (auto *cmd = dynamic_cast<SceneCommand *>(
            const_cast<QUndoCommand *>(stack->command(i)))) {
      cmd->updateEntityIds(idMap);
    }
  }

  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// CutCommand
CutCommand::CutCommand(MainWindow *w, const QList<Entity> &sel, QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entities(sel) {
  setText(QObject::tr("Cut Entities"));
  Scene &scene = w->canvas()->scene();
  for (Entity e : sel) {
    QJsonObject snapshot = snapshotEntity(scene, e);
    snapshot["id"] = (qint64)e.id();
    m_entitiesData.append(snapshot);
  }
}

void CutCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  for (int i = 0; i < m_entities.size(); ++i) {
    if (idMap.contains(m_entities[i].id())) {
      m_entities[i] = idMap[m_entities[i].id()];
    }
  }
}

void CutCommand::redo() {
  destroyList(m_entities);
  m_entities.clear();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void CutCommand::undo() {
  // Use the main helper that updates the stack
  m_entities = recreateList(m_mainWindow, m_entitiesData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// DeleteCommand
DeleteCommand::DeleteCommand(MainWindow *w, const QList<Entity> &ents,
                             QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entities(ents) {
  setText(QObject::tr("Delete Entities"));
  Scene &scene = w->canvas()->scene();
  for (Entity e : ents) {
    QJsonObject snapshot = snapshotEntity(scene, e);
    snapshot["id"] = (qint64)e.id();
    m_entitiesData.append(snapshot);
  }
}

void DeleteCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  for (int i = 0; i < m_entities.size(); ++i) {
    if (idMap.contains(m_entities[i].id())) {
      m_entities[i] = idMap[m_entities[i].id()];
    }
  }
}

void DeleteCommand::redo() {
  destroyList(m_entities);
  m_entities.clear();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void DeleteCommand::undo() {
  m_entities = recreateList(m_mainWindow, m_entitiesData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// MoveEntityCommand
MoveEntityCommand::MoveEntityCommand(MainWindow *w, Entity e, float oldX,
                                     float oldY, float oldR, float newX,
                                     float newY, float newR, QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldX(oldX), m_oldY(oldY),
      m_oldRot(oldR), m_newX(newX), m_newY(newY), m_newRot(newR) {
  setText(QObject::tr("Move Entity"));
}

void MoveEntityCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}

void MoveEntityCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<TransformComponent>()) {
    auto &t = m_entity.get_mut<TransformComponent>();
    t.x = m_oldX;
    t.y = m_oldY;
    t.rotation = m_oldRot;
    m_mainWindow->canvas()->update();
  }
}

void MoveEntityCommand::redo() {
  if (m_entity.is_alive() && m_entity.has<TransformComponent>()) {
    auto &t = m_entity.get_mut<TransformComponent>();
    t.x = m_newX;
    t.y = m_newY;
    t.rotation = m_newRot;
    m_mainWindow->canvas()->update();
  }
}

// ChangeNameCommand
ChangeNameCommand::ChangeNameCommand(MainWindow *w, Entity e,
                                     const std::string &oldN,
                                     const std::string &newN, QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldName(oldN),
      m_newName(newN) {
  setText(QObject::tr("Change Entity Name"));
}
void ChangeNameCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}
void ChangeNameCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<NameComponent>()) {
    auto &n = m_entity.get_mut<NameComponent>();
    n.name = m_oldName;
    m_mainWindow->sceneModel()->refresh();
    m_mainWindow->canvas()->update();
  }
}
void ChangeNameCommand::redo() {
  if (m_entity.is_alive() && m_entity.has<NameComponent>()) {
    auto &n = m_entity.get_mut<NameComponent>();
    n.name = m_newName;
    m_mainWindow->sceneModel()->refresh();
    m_mainWindow->canvas()->update();
  }
}

// ChangeTransformCommand
ChangeTransformCommand::ChangeTransformCommand(MainWindow *w, Entity e,
                                               float oX, float oY, float oR,
                                               float oSx, float oSy, float nX,
                                               float nY, float nR, float nSx,
                                               float nSy, QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldX(oX), m_oldY(oY),
      m_oldRot(oR), m_oldSx(oSx), m_oldSy(oSy), m_newX(nX), m_newY(nY),
      m_newRot(nR), m_newSx(nSx), m_newSy(nSy) {
  setText(QObject::tr("Change Entity Transform"));
}
void ChangeTransformCommand::updateEntityIds(
    const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}
void ChangeTransformCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<TransformComponent>()) {
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
  if (m_entity.is_alive() && m_entity.has<TransformComponent>()) {
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

// ChangeMaterialCommand
ChangeMaterialCommand::ChangeMaterialCommand(MainWindow *w, Entity e,
                                             SkColor oCol, bool oFill,
                                             bool oStroke, float oSw, bool oAA,
                                             SkColor nCol, bool nFill,
                                             bool nStroke, float nSw, bool nAA,
                                             QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldColor(oCol),
      m_oldFill(oFill), m_oldStroke(oStroke), m_oldWidth(oSw), m_oldAA(oAA),
      m_newColor(nCol), m_newFill(nFill), m_newStroke(nStroke), m_newWidth(nSw),
      m_newAA(nAA) {
  setText(QObject::tr("Change Entity Material"));
}
void ChangeMaterialCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}
void ChangeMaterialCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<MaterialComponent>()) {
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
  if (m_entity.is_alive() && m_entity.has<MaterialComponent>()) {
    auto &m = m_entity.get_mut<MaterialComponent>();
    m.color = m_newColor;
    m.isFilled = m_newFill;
    m.isStroked = m_newStroke;
    m.strokeWidth = m_newWidth;
    m.antiAliased = m_newAA;
    m_mainWindow->canvas()->update();
  }
}

// ChangeAnimationCommand
ChangeAnimationCommand::ChangeAnimationCommand(MainWindow *w, Entity e,
                                               float oEntry, float oExit,
                                               float nEntry, float nExit,
                                               QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldEntry(oEntry),
      m_oldExit(oExit), m_newEntry(nEntry), m_newExit(nExit) {
  setText(QObject::tr("Change Entity Animation"));
}
void ChangeAnimationCommand::updateEntityIds(
    const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}
void ChangeAnimationCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<AnimationComponent>()) {
    auto &a = m_entity.get_mut<AnimationComponent>();
    a.entryTime = m_oldEntry;
    a.exitTime = m_oldExit;
    m_mainWindow->canvas()->update();
  }
}
void ChangeAnimationCommand::redo() {
  if (m_entity.is_alive() && m_entity.has<AnimationComponent>()) {
    auto &a = m_entity.get_mut<AnimationComponent>();
    a.entryTime = m_newEntry;
    a.exitTime = m_newExit;
    m_mainWindow->canvas()->update();
  }
}

// ChangeScriptCommand
ChangeScriptCommand::ChangeScriptCommand(
    MainWindow *w, Entity e, const std::string &oPath,
    const std::string &oStart, const std::string &oUpdate,
    const std::string &oDestroy, const std::string &nPath,
    const std::string &nStart, const std::string &nUpdate,
    const std::string &nDestroy, QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldPath(oPath),
      m_oldStart(oStart), m_oldUpdate(oUpdate), m_oldDestroy(oDestroy),
      m_newPath(nPath), m_newStart(nStart), m_newUpdate(nUpdate),
      m_newDestroy(nDestroy) {
  setText(QObject::tr("Change Entity Script"));
}
void ChangeScriptCommand::updateEntityIds(const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}
void ChangeScriptCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<ScriptComponent>()) {
    auto &s = m_entity.get_mut<ScriptComponent>();
    s.scriptPath = m_oldPath;
    s.startFunction = m_oldStart;
    s.updateFunction = m_oldUpdate;
    s.destroyFunction = m_oldDestroy;
    m_mainWindow->canvas()->update();
  }
}
void ChangeScriptCommand::redo() {
  if (m_entity.is_alive() && m_entity.has<ScriptComponent>()) {
    auto &s = m_entity.get_mut<ScriptComponent>();
    s.scriptPath = m_newPath;
    s.startFunction = m_newStart;
    s.updateFunction = m_newUpdate;
    s.destroyFunction = m_newDestroy;
    m_mainWindow->canvas()->update();
  }
}

// ChangeShapePropertyCommand
ChangeShapePropertyCommand::ChangeShapePropertyCommand(MainWindow *w, Entity e,
                                                       const QJsonObject &oldP,
                                                       const QJsonObject &newP,
                                                       QUndoCommand *p)
    : SceneCommand(p), m_mainWindow(w), m_entity(e), m_oldProps(oldP),
      m_newProps(newP) {
  setText(QObject::tr("Change Shape Property"));
}
void ChangeShapePropertyCommand::updateEntityIds(
    const QMap<qint64, Entity> &idMap) {
  if (idMap.contains(m_entity.id()))
    m_entity = idMap[m_entity.id()];
}
void ChangeShapePropertyCommand::undo() {
  if (m_entity.is_alive() && m_entity.has<ShapeComponent>()) {
    auto &sc = m_entity.get_mut<ShapeComponent>();
    if (sc.shape) {
      sc.shape->deserialize(m_oldProps);
      m_mainWindow->canvas()->update();
    }
  }
}
void ChangeShapePropertyCommand::redo() {
  if (m_entity.is_alive() && m_entity.has<ShapeComponent>()) {
    auto &sc = m_entity.get_mut<ShapeComponent>();
    if (sc.shape) {
      sc.shape->deserialize(m_newProps);
      m_mainWindow->canvas()->update();
    }
  }
}
