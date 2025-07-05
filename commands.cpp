#include "commands.h"
#include "ecs.h"
#include "serialization.h"
#include "window.h"

// serialise a single entity
inline QJsonObject snapshotEntity(Scene &scene, Entity e) {
  return serializeEntity(scene, e);
}

// restore into *exactly the same handle* if possible
inline void restoreInto(Scene &scene, Entity e, const QJsonObject &json) {
  // rebuild all components on top of the existing handle
  scene.reg.destroy(e);
  ComponentRegistry::instance().apply(scene, e, json);
}

// create a *fresh* entity
inline Entity createFrom(Scene &scene, const QJsonObject &json) {
  Entity e = scene.reg.create();
  ComponentRegistry::instance().apply(scene, e, json);
  return e;
}

/* recreates the entity, returns its (new) id */
Entity restoreEntity(Scene &scene, const QJsonObject &json) {
  Entity e = scene.reg.create();
  ComponentRegistry::instance().apply(scene, e, json);
  return e;
}

AddEntityCommand::AddEntityCommand(MainWindow *w, Entity e, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e),
      m_entityData(snapshotEntity(w->canvas()->scene(), e)) {
  setText(QObject::tr("Add Entity"));
  m_firstRedo = true;
}

void AddEntityCommand::undo() {
  m_mainWindow->canvas()->scene().reg.destroy(m_entity);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void AddEntityCommand::redo() {
  if (m_firstRedo) { // nothing to do – entity already there
    m_firstRedo = false;
    return;
  }
  auto &scene = m_mainWindow->canvas()->scene();
  m_entity = createFrom(scene, m_entityData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

RemoveEntityCommand::RemoveEntityCommand(MainWindow *w, Entity e,
                                         QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entity(e),
      m_entityData(snapshotEntity(w->canvas()->scene(), e)) {
  setText(QObject::tr("Remove Entity"));
}

void RemoveEntityCommand::redo() {
  m_mainWindow->canvas()->scene().reg.destroy(m_entity);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void RemoveEntityCommand::undo() {
  auto &scene = m_mainWindow->canvas()->scene();
  restoreInto(scene, m_entity, m_entityData);

  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void destroyList(Scene &scene, const QList<Entity> &list) {
  for (Entity e : list)
    scene.reg.destroy(e);
}

QList<Entity> recreateList(Scene &scene, const QList<QJsonObject> &jsons) {
  QList<Entity> out;
  out.reserve(jsons.size());
  for (const auto &j : jsons)
    out.append(createFrom(scene, j));
  return out;
}

CutCommand::CutCommand(MainWindow *w, const QList<Entity> &sel, QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(w), m_entities(sel) {
  setText(QObject::tr("Cut Entities"));
  auto &scene = w->canvas()->scene();
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

// MoveEntityCommand
MoveEntityCommand::MoveEntityCommand(MainWindow *mainWindow, Entity entity,
                                     float oldX, float oldY, float oldRotation,
                                     float newX, float newY, float newRotation,
                                     QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(mainWindow), m_entity(entity),
      m_oldX(oldX), m_oldY(oldY), m_oldRotation(oldRotation), m_newX(newX),
      m_newY(newY), m_newRotation(newRotation) {
  setText(QObject::tr("Move Entity"));
}

void MoveEntityCommand::undo() {
  if (auto *transform =
          m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
              m_entity)) {
    transform->x = m_oldX;
    transform->y = m_oldY;
    transform->rotation = m_oldRotation;
    m_mainWindow->canvas()->update();
  }
}

void MoveEntityCommand::redo() {
  if (auto *transform =
          m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
              m_entity)) {
    transform->x = m_newX;
    transform->y = m_newY;
    transform->rotation = m_newRotation;
    m_mainWindow->canvas()->update();
  }
}

// DeleteCommand
DeleteCommand::DeleteCommand(MainWindow *win, const QList<Entity> &ents,
                             QUndoCommand *p)
    : QUndoCommand(p), m_mainWindow(win), m_entities(ents) {
  setText(QObject::tr("Delete Entities"));
  auto &scene = win->canvas()->scene();
  for (Entity e : m_entities)
    m_entitiesData.append(snapshotEntity(scene, e));
}

void DeleteCommand::redo() {
  auto &scene = m_mainWindow->canvas()->scene();
  destroyList(scene, m_entities);
  m_entities.clear();
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void DeleteCommand::undo() {
  auto &scene = m_mainWindow->canvas()->scene();
  m_entities = recreateList(scene, m_entitiesData);
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// ChangeNameCommand
ChangeNameCommand::ChangeNameCommand(MainWindow *window, Entity entity,
                                     const std::string &oldName,
                                     const std::string &newName,
                                     QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity),
      m_oldName(oldName), m_newName(newName) {
  setText(QObject::tr("Change Entity Name"));
}

void ChangeNameCommand::undo() {
  if (auto *name =
          m_mainWindow->canvas()->scene().reg.get<NameComponent>(m_entity)) {
    name->name = m_oldName;
    m_mainWindow->sceneModel()->refresh();
    m_mainWindow->canvas()->update();
  }
}

void ChangeNameCommand::redo() {
  if (auto *name =
          m_mainWindow->canvas()->scene().reg.get<NameComponent>(m_entity)) {
    name->name = m_newName;
    m_mainWindow->sceneModel()->refresh();
    m_mainWindow->canvas()->update();
  }
}

// ChangeTransformCommand
ChangeTransformCommand::ChangeTransformCommand(
    MainWindow *window, Entity entity, float oldX, float oldY,
    float oldRotation, float oldSx, float oldSy, float newX, float newY,
    float newRotation, float newSx, float newSy, QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity),
      m_oldX(oldX), m_oldY(oldY), m_oldRotation(oldRotation), m_oldSx(oldSx),
      m_oldSy(oldSy), m_newX(newX), m_newY(newY), m_newRotation(newRotation),
      m_newSx(newSx), m_newSy(newSy) {
  setText(QObject::tr("Change Entity Transform"));
}

void ChangeTransformCommand::undo() {
  if (auto *transform =
          m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
              m_entity)) {
    transform->x = m_oldX;
    transform->y = m_oldY;
    transform->rotation = m_oldRotation;
    transform->sx = m_oldSx;
    transform->sy = m_oldSy;
    m_mainWindow->canvas()->update();
  }
}

void ChangeTransformCommand::redo() {
  if (auto *transform =
          m_mainWindow->canvas()->scene().reg.get<TransformComponent>(
              m_entity)) {
    transform->x = m_newX;
    transform->y = m_newY;
    transform->rotation = m_newRotation;
    transform->sx = m_newSx;
    transform->sy = m_newSy;
    m_mainWindow->canvas()->update();
  }
}

bool ChangeTransformCommand::mergeWith(const QUndoCommand *other) {
  // Ensure the other command is the same type
  if (other->id() != Id) {
    return false;
  }

  // Cast it to our specific type
  const auto *otherCmd = static_cast<const ChangeTransformCommand *>(other);

  // Ensure it's for the same entity
  if (m_entity != otherCmd->m_entity) {
    return false;
  }

  // It's a match! Absorb the "new" state from the other command.
  // Our own "old" state is preserved, representing the true beginning of the
  // action.
  m_newX = otherCmd->m_newX;
  m_newY = otherCmd->m_newY;
  m_newRotation = otherCmd->m_newRotation;
  m_newSx = otherCmd->m_newSx;
  m_newSy = otherCmd->m_newSy;

  // Return true to tell the undo stack the merge was successful.
  return true;
}
// ChangeMaterialCommand
ChangeMaterialCommand::ChangeMaterialCommand(
    MainWindow *window, Entity entity, SkColor oldColor, bool oldIsFilled,
    bool oldIsStroked, float oldStrokeWidth, bool oldAntiAliased,
    SkColor newColor, bool newIsFilled, bool newIsStroked, float newStrokeWidth,
    bool newAntiAliased, QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity),
      m_oldColor(oldColor), m_oldIsFilled(oldIsFilled),
      m_oldIsStroked(oldIsStroked), m_oldStrokeWidth(oldStrokeWidth),
      m_oldAntiAliased(oldAntiAliased), m_newColor(newColor),
      m_newIsFilled(newIsFilled), m_newIsStroked(newIsStroked),
      m_newStrokeWidth(newStrokeWidth), m_newAntiAliased(newAntiAliased) {
  setText(QObject::tr("Change Entity Material"));
}

void ChangeMaterialCommand::undo() {
  if (auto *material =
          m_mainWindow->canvas()->scene().reg.get<MaterialComponent>(
              m_entity)) {
    material->color = m_oldColor;
    material->isFilled = m_oldIsFilled;
    material->isStroked = m_oldIsStroked;
    material->strokeWidth = m_oldStrokeWidth;
    material->antiAliased = m_oldAntiAliased;
    m_mainWindow->canvas()->update();
  }
}

void ChangeMaterialCommand::redo() {
  if (auto *material =
          m_mainWindow->canvas()->scene().reg.get<MaterialComponent>(
              m_entity)) {
    material->color = m_newColor;
    material->isFilled = m_newIsFilled;
    material->isStroked = m_newIsStroked;
    material->strokeWidth = m_newStrokeWidth;
    material->antiAliased = m_newAntiAliased;
    m_mainWindow->canvas()->update();
  }
}

// ChangeAnimationCommand
ChangeAnimationCommand::ChangeAnimationCommand(
    MainWindow *window, Entity entity, float oldEntryTime, float oldExitTime,
    float newEntryTime, float newExitTime, QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity),
      m_oldEntryTime(oldEntryTime), m_oldExitTime(oldExitTime),
      m_newEntryTime(newEntryTime), m_newExitTime(newExitTime) {
  setText(QObject::tr("Change Entity Animation"));
}

void ChangeAnimationCommand::undo() {
  if (auto *animation =
          m_mainWindow->canvas()->scene().reg.get<AnimationComponent>(
              m_entity)) {
    animation->entryTime = m_oldEntryTime;
    animation->exitTime = m_oldExitTime;
    m_mainWindow->canvas()->update();
  }
}

void ChangeAnimationCommand::redo() {
  if (auto *animation =
          m_mainWindow->canvas()->scene().reg.get<AnimationComponent>(
              m_entity)) {
    animation->entryTime = m_newEntryTime;
    animation->exitTime = m_newExitTime;
    m_mainWindow->canvas()->update();
  }
}

// ChangeScriptCommand
ChangeScriptCommand::ChangeScriptCommand(
    MainWindow *window, Entity entity, const std::string &oldScriptPath,
    const std::string &oldStartFunction, const std::string &oldUpdateFunction,
    const std::string &oldDestroyFunction, const std::string &newScriptPath,
    const std::string &newStartFunction, const std::string &newUpdateFunction,
    const std::string &newDestroyFunction, QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity),
      m_oldScriptPath(oldScriptPath), m_oldStartFunction(oldStartFunction),
      m_oldUpdateFunction(oldUpdateFunction),
      m_oldDestroyFunction(oldDestroyFunction), m_newScriptPath(newScriptPath),
      m_newStartFunction(newStartFunction),
      m_newUpdateFunction(newUpdateFunction),
      m_newDestroyFunction(newDestroyFunction) {
  setText(QObject::tr("Change Entity Script"));
}

void ChangeScriptCommand::undo() {
  if (auto *script =
          m_mainWindow->canvas()->scene().reg.get<ScriptComponent>(m_entity)) {
    script->scriptPath = m_oldScriptPath;
    script->startFunction = m_oldStartFunction;
    script->updateFunction = m_oldUpdateFunction;
    script->destroyFunction = m_oldDestroyFunction;
    m_mainWindow->canvas()->update();
  }
}

void ChangeScriptCommand::redo() {
  if (auto *script =
          m_mainWindow->canvas()->scene().reg.get<ScriptComponent>(m_entity)) {
    script->scriptPath = m_newScriptPath;
    script->startFunction = m_newStartFunction;
    script->updateFunction = m_newUpdateFunction;
    script->destroyFunction = m_newDestroyFunction;
    m_mainWindow->canvas()->update();
  }
}

// ChangeShapePropertyCommand
ChangeShapePropertyCommand::ChangeShapePropertyCommand(
    MainWindow *window, Entity entity, ShapeComponent::Kind kind,
    const QJsonObject &oldProperties, const QJsonObject &newProperties,
    QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity),
      m_kind(kind), m_oldProperties(oldProperties),
      m_newProperties(newProperties) {
  setText(QObject::tr("Change Entity Shape Property"));
}

void ChangeShapePropertyCommand::undo() {
  if (auto *shapeComp =
          m_mainWindow->canvas()->scene().reg.get<ShapeComponent>(m_entity)) {
    shapeComp->setProperties(m_oldProperties);
    m_mainWindow->canvas()->update();
  }
}

void ChangeShapePropertyCommand::redo() {
  if (auto *shapeComp =
          m_mainWindow->canvas()->scene().reg.get<ShapeComponent>(m_entity)) {
    shapeComp->setProperties(m_newProperties);
    m_mainWindow->canvas()->update();
  }
}
