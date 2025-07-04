#include "commands.h"
#include "ecs.h"
#include "window.h"

// AddEntityCommand
AddEntityCommand::AddEntityCommand(MainWindow *mainWindow, Entity entity,
                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(mainWindow), m_entity(entity) {
  setText(QObject::tr("Add Entity"));
  // Serialize the entity's current state for undo
  QJsonObject entityObject;
  auto &m_scene = m_mainWindow->canvas()->scene();
  if (auto *name =
          m_mainWindow->canvas()->scene().reg.get<NameComponent>(m_entity)) {
    entityObject["NameComponent"] = name->name.c_str();
  }
  if (auto *transform = m_scene.reg.get<TransformComponent>(m_entity)) {
    QJsonObject transformObject;
    transformObject["x"] = transform->x;
    transformObject["y"] = transform->y;
    transformObject["rotation"] = transform->rotation;
    transformObject["sx"] = transform->sx;
    transformObject["sy"] = transform->sy;
    entityObject["TransformComponent"] = transformObject;
  }
  if (auto *material = m_scene.reg.get<MaterialComponent>(m_entity)) {
    QJsonObject materialObject;
    materialObject["color"] = (qint64)material->color;
    materialObject["isFilled"] = material->isFilled;
    materialObject["isStroked"] = material->isStroked;
    materialObject["strokeWidth"] = material->strokeWidth;
    materialObject["antiAliased"] = material->antiAliased;
    entityObject["MaterialComponent"] = materialObject;
  }
  if (auto *animation = m_scene.reg.get<AnimationComponent>(m_entity)) {
    QJsonObject animationObject;
    animationObject["entryTime"] = animation->entryTime;
    animationObject["exitTime"] = animation->exitTime;
    entityObject["AnimationComponent"] = animationObject;
  }
  if (auto *script = m_scene.reg.get<ScriptComponent>(m_entity)) {
    QJsonObject scriptObject;
    scriptObject["scriptPath"] = script->scriptPath.c_str();
    scriptObject["startFunction"] = script->startFunction.c_str();
    scriptObject["updateFunction"] = script->updateFunction.c_str();
    scriptObject["destroyFunction"] = script->destroyFunction.c_str();
    entityObject["ScriptComponent"] = scriptObject;
  }
  if (m_scene.reg.has<SceneBackgroundComponent>(m_entity)) {
    entityObject["SceneBackgroundComponent"] = true;
  }
  if (auto *shape = m_scene.reg.get<ShapeComponent>(m_entity)) {
    QJsonObject shapeObject;
    shapeObject["kind"] = (int)shape->kind;
    shapeObject["properties"] = shape->getProperties().toJsonObject();
    entityObject["ShapeComponent"] = shapeObject;
  }
  m_entityData = entityObject;
}

void AddEntityCommand::undo() {
  m_mainWindow->canvas()->scene().reg.destroy(m_entity);
  m_entity = kInvalidEntity; // Invalidate entity ID after destruction
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void AddEntityCommand::redo() {
  // Recreate the entity and its components from m_entityData
  // This assumes the entity ID can be reused or is not critical for redo
  // If entity ID must be preserved, a more complex mapping is needed.
  // For simplicity, we'll create a new entity and update m_entity
  // if this command is redone after an undo.
  auto &m_scene = m_mainWindow->canvas()->scene();
  if (m_entity == kInvalidEntity) {
    m_entity = m_scene.reg.create();
  }

  if (m_entityData.contains("NameComponent") &&
      m_entityData["NameComponent"].isString()) {
    m_scene.reg.emplace<NameComponent>(
        m_entity,
        NameComponent{m_entityData["NameComponent"].toString().toStdString()});
  }
  if (m_entityData.contains("TransformComponent") &&
      m_entityData["TransformComponent"].isObject()) {
    QJsonObject transformObject = m_entityData["TransformComponent"].toObject();
    m_scene.reg.emplace<TransformComponent>(
        m_entity,
        TransformComponent{(float)transformObject["x"].toDouble(),
                           (float)transformObject["y"].toDouble(),
                           (float)transformObject["rotation"].toDouble(),
                           (float)transformObject["sx"].toDouble(),
                           (float)transformObject["sy"].toDouble()});
  }
  if (m_entityData.contains("MaterialComponent") &&
      m_entityData["MaterialComponent"].isObject()) {
    QJsonObject materialObject = m_entityData["MaterialComponent"].toObject();
    m_scene.reg.emplace<MaterialComponent>(
        m_entity,
        MaterialComponent{
            (SkColor)materialObject["color"].toVariant().toULongLong(),
            materialObject["isFilled"].toBool(),
            materialObject["isStroked"].toBool(),
            (float)materialObject["strokeWidth"].toDouble(),
            materialObject["antiAliased"].toBool()});
  }
  if (m_entityData.contains("AnimationComponent") &&
      m_entityData["AnimationComponent"].isObject()) {
    QJsonObject animationObject = m_entityData["AnimationComponent"].toObject();
    m_scene.reg.emplace<AnimationComponent>(
        m_entity,
        AnimationComponent{(float)animationObject["entryTime"].toDouble(),
                           (float)animationObject["exitTime"].toDouble()});
  }
  if (m_entityData.contains("ScriptComponent") &&
      m_entityData["ScriptComponent"].isObject()) {
    QJsonObject scriptObject = m_entityData["ScriptComponent"].toObject();
    m_scene.reg.emplace<ScriptComponent>(
        m_entity,
        ScriptComponent{
            scriptObject["scriptPath"].toString().toStdString(),
            scriptObject["startFunction"].toString().toStdString(),
            scriptObject["updateFunction"].toString().toStdString(),
            scriptObject["destroyFunction"].toString().toStdString()});
  }
  if (m_entityData.contains("SceneBackgroundComponent") &&
      m_entityData["SceneBackgroundComponent"].toBool()) {
    m_scene.reg.emplace<SceneBackgroundComponent>(m_entity);
  }
  if (m_entityData.contains("ShapeComponent") &&
      m_entityData["ShapeComponent"].isObject()) {
    QJsonObject shapeObject = m_entityData["ShapeComponent"].toObject();
    ShapeComponent::Kind kind =
        (ShapeComponent::Kind)shapeObject["kind"].toInt();
    ShapeComponent shapeComp{kind, std::monostate{}};

    if (shapeObject.contains("properties") &&
        shapeObject["properties"].isObject()) {
      QJsonObject propsObject = shapeObject["properties"].toObject();
      switch (kind) {
      case ShapeComponent::Kind::Rectangle: {
        RectangleProperties props;
        const QMetaObject &metaObject = props.staticMetaObject;
        for (int i = metaObject.propertyOffset();
             i < metaObject.propertyCount(); ++i) {
          QMetaProperty metaProperty = metaObject.property(i);
          if (propsObject.contains(metaProperty.name())) {
            metaProperty.writeOnGadget(
                &props, propsObject[metaProperty.name()].toVariant());
          }
        }
        shapeComp.properties = props;
        break;
      }
      case ShapeComponent::Kind::Circle: {
        CircleProperties props;
        const QMetaObject &metaObject = props.staticMetaObject;
        for (int i = metaObject.propertyOffset();
             i < metaObject.propertyCount(); ++i) {
          QMetaProperty metaProperty = metaObject.property(i);
          if (propsObject.contains(metaProperty.name())) {
            metaProperty.writeOnGadget(
                &props, propsObject[metaProperty.name()].toVariant());
          }
        }
        shapeComp.properties = props;
        break;
      }
      }
    } else {
      // If properties are empty or missing, initialize with defaults
      // based on kind
      if (kind == ShapeComponent::Kind::Rectangle) {
        shapeComp.properties.emplace<RectangleProperties>();
      } else if (kind == ShapeComponent::Kind::Circle) {
        shapeComp.properties.emplace<CircleProperties>();
      }
    }
    m_scene.reg.emplace<ShapeComponent>(m_entity, shapeComp);
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// RemoveEntityCommand
RemoveEntityCommand::RemoveEntityCommand(MainWindow *window, Entity entity,
                                         QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entity(entity) {
  setText(QObject::tr("Remove Entity"));
  auto &m_scene = m_mainWindow->canvas()->scene();
  // Serialize the entity's current state for undo
  QJsonObject entityObject;
  if (auto *name = m_scene.reg.get<NameComponent>(m_entity)) {
    entityObject["NameComponent"] = name->name.c_str();
  }
  if (auto *transform = m_scene.reg.get<TransformComponent>(m_entity)) {
    QJsonObject transformObject;
    transformObject["x"] = transform->x;
    transformObject["y"] = transform->y;
    transformObject["rotation"] = transform->rotation;
    transformObject["sx"] = transform->sx;
    transformObject["sy"] = transform->sy;
    entityObject["TransformComponent"] = transformObject;
  }
  if (auto *material = m_scene.reg.get<MaterialComponent>(m_entity)) {
    QJsonObject materialObject;
    materialObject["color"] = (qint64)material->color;
    materialObject["isFilled"] = material->isFilled;
    materialObject["isStroked"] = material->isStroked;
    materialObject["strokeWidth"] = material->strokeWidth;
    materialObject["antiAliased"] = material->antiAliased;
    entityObject["MaterialComponent"] = materialObject;
  }
  if (auto *animation = m_scene.reg.get<AnimationComponent>(m_entity)) {
    QJsonObject animationObject;
    animationObject["entryTime"] = animation->entryTime;
    animationObject["exitTime"] = animation->exitTime;
    entityObject["AnimationComponent"] = animationObject;
  }
  if (auto *script = m_scene.reg.get<ScriptComponent>(m_entity)) {
    QJsonObject scriptObject;
    scriptObject["scriptPath"] = script->scriptPath.c_str();
    scriptObject["startFunction"] = script->startFunction.c_str();
    scriptObject["updateFunction"] = script->updateFunction.c_str();
    scriptObject["destroyFunction"] = script->destroyFunction.c_str();
    entityObject["ScriptComponent"] = scriptObject;
  }
  if (m_scene.reg.has<SceneBackgroundComponent>(m_entity)) {
    entityObject["SceneBackgroundComponent"] = true;
  }
  if (auto *shape = m_scene.reg.get<ShapeComponent>(m_entity)) {
    QJsonObject shapeObject;
    shapeObject["kind"] = (int)shape->kind;
    shapeObject["properties"] = shape->getProperties().toJsonObject();
    entityObject["ShapeComponent"] = shapeObject;
  }
  m_entityData = entityObject;
}

void RemoveEntityCommand::undo() {
  // Recreate the entity and its components from m_entityData
  // This assumes the entity ID can be reused or is not critical for undo
  // If entity ID must be preserved, a more complex mapping is needed.
  // For simplicity, we'll create a new entity and update m_entity
  // if this command is redone after an undo.
  auto &m_scene = m_mainWindow->canvas()->scene();
  if (m_entity == kInvalidEntity) {
    m_entity = m_scene.reg.create();
  }

  if (m_entityData.contains("NameComponent") &&
      m_entityData["NameComponent"].isString()) {
    m_scene.reg.emplace<NameComponent>(
        m_entity,
        NameComponent{m_entityData["NameComponent"].toString().toStdString()});
  }
  if (m_entityData.contains("TransformComponent") &&
      m_entityData["TransformComponent"].isObject()) {
    QJsonObject transformObject = m_entityData["TransformComponent"].toObject();
    m_scene.reg.emplace<TransformComponent>(
        m_entity,
        TransformComponent{(float)transformObject["x"].toDouble(),
                           (float)transformObject["y"].toDouble(),
                           (float)transformObject["rotation"].toDouble(),
                           (float)transformObject["sx"].toDouble(),
                           (float)transformObject["sy"].toDouble()});
  }
  if (m_entityData.contains("MaterialComponent") &&
      m_entityData["MaterialComponent"].isObject()) {
    QJsonObject materialObject = m_entityData["MaterialComponent"].toObject();
    m_scene.reg.emplace<MaterialComponent>(
        m_entity,
        MaterialComponent{
            (SkColor)materialObject["color"].toVariant().toULongLong(),
            materialObject["isFilled"].toBool(),
            materialObject["isStroked"].toBool(),
            (float)materialObject["strokeWidth"].toDouble(),
            materialObject["antiAliased"].toBool()});
  }
  if (m_entityData.contains("AnimationComponent") &&
      m_entityData["AnimationComponent"].isObject()) {
    QJsonObject animationObject = m_entityData["AnimationComponent"].toObject();
    m_scene.reg.emplace<AnimationComponent>(
        m_entity,
        AnimationComponent{(float)animationObject["entryTime"].toDouble(),
                           (float)animationObject["exitTime"].toDouble()});
  }
  if (m_entityData.contains("ScriptComponent") &&
      m_entityData["ScriptComponent"].isObject()) {
    QJsonObject scriptObject = m_entityData["ScriptComponent"].toObject();
    m_scene.reg.emplace<ScriptComponent>(
        m_entity,
        ScriptComponent{
            scriptObject["scriptPath"].toString().toStdString(),
            scriptObject["startFunction"].toString().toStdString(),
            scriptObject["updateFunction"].toString().toStdString(),
            scriptObject["destroyFunction"].toString().toStdString()});
  }
  if (m_entityData.contains("SceneBackgroundComponent") &&
      m_entityData["SceneBackgroundComponent"].toBool()) {
    m_scene.reg.emplace<SceneBackgroundComponent>(m_entity);
  }
  if (m_entityData.contains("ShapeComponent") &&
      m_entityData["ShapeComponent"].isObject()) {
    QJsonObject shapeObject = m_entityData["ShapeComponent"].toObject();
    ShapeComponent::Kind kind =
        (ShapeComponent::Kind)shapeObject["kind"].toInt();
    ShapeComponent shapeComp{kind, std::monostate{}};

    if (shapeObject.contains("properties") &&
        shapeObject["properties"].isObject()) {
      QJsonObject propsObject = shapeObject["properties"].toObject();
      switch (kind) {
      case ShapeComponent::Kind::Rectangle: {
        RectangleProperties props;
        const QMetaObject &metaObject = props.staticMetaObject;
        for (int i = metaObject.propertyOffset();
             i < metaObject.propertyCount(); ++i) {
          QMetaProperty metaProperty = metaObject.property(i);
          if (propsObject.contains(metaProperty.name())) {
            metaProperty.writeOnGadget(
                &props, propsObject[metaProperty.name()].toVariant());
          }
        }
        shapeComp.properties = props;
        break;
      }
      case ShapeComponent::Kind::Circle: {
        CircleProperties props;
        const QMetaObject &metaObject = props.staticMetaObject;
        for (int i = metaObject.propertyOffset();
             i < metaObject.propertyCount(); ++i) {
          QMetaProperty metaProperty = metaObject.property(i);
          if (propsObject.contains(metaProperty.name())) {
            metaProperty.writeOnGadget(
                &props, propsObject[metaProperty.name()].toVariant());
          }
        }
        shapeComp.properties = props;
        break;
      }
      }
    } else {
      // If properties are empty or missing, initialize with defaults
      // based on kind
      if (kind == ShapeComponent::Kind::Rectangle) {
        shapeComp.properties.emplace<RectangleProperties>();
      } else if (kind == ShapeComponent::Kind::Circle) {
        shapeComp.properties.emplace<CircleProperties>();
      }
    }
    m_scene.reg.emplace<ShapeComponent>(m_entity, shapeComp);
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void RemoveEntityCommand::redo() {
  m_mainWindow->canvas()->scene().reg.destroy(m_entity);
  m_entity = kInvalidEntity; // Invalidate entity ID after destruction
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

// CutCommand
CutCommand::CutCommand(MainWindow *window, const QList<Entity> &entities,
                       QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entities(entities) {
  setText(QObject::tr("Cut Entities"));
  auto &m_scene = m_mainWindow->canvas()->scene();
  for (Entity e : m_entities) {
    QJsonObject entityObject;
    // Serialize entity data
    if (auto *name = m_scene.reg.get<NameComponent>(e)) {
      entityObject["NameComponent"] = name->name.c_str();
    }
    if (auto *transform = m_scene.reg.get<TransformComponent>(e)) {
      QJsonObject transformObject;
      transformObject["x"] = transform->x;
      transformObject["y"] = transform->y;
      transformObject["rotation"] = transform->rotation;
      transformObject["sx"] = transform->sx;
      transformObject["sy"] = transform->sy;
      entityObject["TransformComponent"] = transformObject;
    }
    if (auto *material = m_scene.reg.get<MaterialComponent>(e)) {
      QJsonObject materialObject;
      materialObject["color"] = (qint64)material->color;
      materialObject["isFilled"] = material->isFilled;
      materialObject["isStroked"] = material->isStroked;
      materialObject["strokeWidth"] = material->strokeWidth;
      materialObject["antiAliased"] = material->antiAliased;
      entityObject["MaterialComponent"] = materialObject;
    }
    if (auto *animation = m_scene.reg.get<AnimationComponent>(e)) {
      QJsonObject animationObject;
      animationObject["entryTime"] = animation->entryTime;
      animationObject["exitTime"] = animation->exitTime;
      entityObject["AnimationComponent"] = animationObject;
    }
    if (auto *script = m_scene.reg.get<ScriptComponent>(e)) {
      QJsonObject scriptObject;
      scriptObject["scriptPath"] = script->scriptPath.c_str();
      scriptObject["startFunction"] = script->startFunction.c_str();
      scriptObject["updateFunction"] = script->updateFunction.c_str();
      scriptObject["destroyFunction"] = script->destroyFunction.c_str();
      entityObject["ScriptComponent"] = scriptObject;
    }
    if (m_scene.reg.has<SceneBackgroundComponent>(e)) {
      entityObject["SceneBackgroundComponent"] = true;
    }
    if (auto *shape = m_scene.reg.get<ShapeComponent>(e)) {
      QJsonObject shapeObject;
      shapeObject["kind"] = (int)shape->kind;
      shapeObject["properties"] = shape->getProperties().toJsonObject();
      entityObject["ShapeComponent"] = shapeObject;
    }
    m_entitiesData.append(entityObject);
  }
}

void CutCommand::undo() {
  auto &m_scene = m_mainWindow->canvas()->scene();
  m_entities.clear(); // Clear existing entities as they will be recreated
  for (const QJsonObject &entityData : m_entitiesData) {
    Entity newEntity = m_scene.reg.create();
    if (entityData.contains("NameComponent") &&
        entityData["NameComponent"].isString()) {
      m_scene.reg.emplace<NameComponent>(
          newEntity,
          NameComponent{entityData["NameComponent"].toString().toStdString()});
    }
    if (entityData.contains("TransformComponent") &&
        entityData["TransformComponent"].isObject()) {
      QJsonObject transformObject = entityData["TransformComponent"].toObject();
      m_scene.reg.emplace<TransformComponent>(
          newEntity,
          TransformComponent{(float)transformObject["x"].toDouble(),
                             (float)transformObject["y"].toDouble(),
                             (float)transformObject["rotation"].toDouble(),
                             (float)transformObject["sx"].toDouble(),
                             (float)transformObject["sy"].toDouble()});
    }
    if (entityData.contains("MaterialComponent") &&
        entityData["MaterialComponent"].isObject()) {
      QJsonObject materialObject = entityData["MaterialComponent"].toObject();
      m_scene.reg.emplace<MaterialComponent>(
          newEntity,
          MaterialComponent{
              (SkColor)materialObject["color"].toVariant().toULongLong(),
              materialObject["isFilled"].toBool(),
              materialObject["isStroked"].toBool(),
              (float)materialObject["strokeWidth"].toDouble(),
              materialObject["antiAliased"].toBool()});
    }
    if (entityData.contains("AnimationComponent") &&
        entityData["AnimationComponent"].isObject()) {
      QJsonObject animationObject = entityData["AnimationComponent"].toObject();
      m_scene.reg.emplace<AnimationComponent>(
          newEntity,
          AnimationComponent{(float)animationObject["entryTime"].toDouble(),
                             (float)animationObject["exitTime"].toDouble()});
    }
    if (entityData.contains("ScriptComponent") &&
        entityData["ScriptComponent"].isObject()) {
      QJsonObject scriptObject = entityData["ScriptComponent"].toObject();
      m_scene.reg.emplace<ScriptComponent>(
          newEntity,
          ScriptComponent{
              scriptObject["scriptPath"].toString().toStdString(),
              scriptObject["startFunction"].toString().toStdString(),
              scriptObject["updateFunction"].toString().toStdString(),
              scriptObject["destroyFunction"].toString().toStdString()});
    }
    if (entityData.contains("SceneBackgroundComponent") &&
        entityData["SceneBackgroundComponent"].toBool()) {
      m_scene.reg.emplace<SceneBackgroundComponent>(newEntity);
    }
    if (entityData.contains("ShapeComponent") &&
        entityData["ShapeComponent"].isObject()) {
      QJsonObject shapeObject = entityData["ShapeComponent"].toObject();
      ShapeComponent::Kind kind =
          (ShapeComponent::Kind)shapeObject["kind"].toInt();
      ShapeComponent shapeComp{kind, std::monostate{}};

      if (shapeObject.contains("properties") &&
          shapeObject["properties"].isObject()) {
        QJsonObject propsObject = shapeObject["properties"].toObject();
        switch (kind) {
        case ShapeComponent::Kind::Rectangle: {
          RectangleProperties props;
          const QMetaObject &metaObject = props.staticMetaObject;
          for (int i = metaObject.propertyOffset();
               i < metaObject.propertyCount(); ++i) {
            QMetaProperty metaProperty = metaObject.property(i);
            if (propsObject.contains(metaProperty.name())) {
              metaProperty.writeOnGadget(
                  &props, propsObject[metaProperty.name()].toVariant());
            }
          }
          shapeComp.properties = props;
          break;
        }
        case ShapeComponent::Kind::Circle: {
          CircleProperties props;
          const QMetaObject &metaObject = props.staticMetaObject;
          for (int i = metaObject.propertyOffset();
               i < metaObject.propertyCount(); ++i) {
            QMetaProperty metaProperty = metaObject.property(i);
            if (propsObject.contains(metaProperty.name())) {
              metaProperty.writeOnGadget(
                  &props, propsObject[metaProperty.name()].toVariant());
            }
          }
          shapeComp.properties = props;
          break;
        }
        }
      } else {
        // If properties are empty or missing, initialize with defaults
        // based on kind
        if (kind == ShapeComponent::Kind::Rectangle) {
          shapeComp.properties.emplace<RectangleProperties>();
        } else if (kind == ShapeComponent::Kind::Circle) {
          shapeComp.properties.emplace<CircleProperties>();
        }
      }
      m_scene.reg.emplace<ShapeComponent>(newEntity, shapeComp);
    }
    m_entities.append(newEntity); // Add the newly created entity to the list
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void CutCommand::redo() {
  auto &m_scene = m_mainWindow->canvas()->scene();
  for (Entity e : m_entities) {
    m_scene.reg.destroy(e);
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

// DeleteCommand
DeleteCommand::DeleteCommand(MainWindow *window, const QList<Entity> &entities,
                             QUndoCommand *parent)
    : QUndoCommand(parent), m_mainWindow(window), m_entities(entities) {
  setText(QObject::tr("Delete Entities"));
  auto &m_scene = m_mainWindow->canvas()->scene();
  for (Entity e : m_entities) {
    QJsonObject entityObject;
    // Serialize entity data
    if (auto *name = m_scene.reg.get<NameComponent>(e)) {
      entityObject["NameComponent"] = name->name.c_str();
    }
    if (auto *transform = m_scene.reg.get<TransformComponent>(e)) {
      QJsonObject transformObject;
      transformObject["x"] = transform->x;
      transformObject["y"] = transform->y;
      transformObject["rotation"] = transform->rotation;
      transformObject["sx"] = transform->sx;
      transformObject["sy"] = transform->sy;
      entityObject["TransformComponent"] = transformObject;
    }
    if (auto *material = m_scene.reg.get<MaterialComponent>(e)) {
      QJsonObject materialObject;
      materialObject["color"] = (qint64)material->color;
      materialObject["isFilled"] = material->isFilled;
      materialObject["isStroked"] = material->isStroked;
      materialObject["strokeWidth"] = material->strokeWidth;
      materialObject["antiAliased"] = material->antiAliased;
      entityObject["MaterialComponent"] = materialObject;
    }
    if (auto *animation = m_scene.reg.get<AnimationComponent>(e)) {
      QJsonObject animationObject;
      animationObject["entryTime"] = animation->entryTime;
      animationObject["exitTime"] = animation->exitTime;
      entityObject["AnimationComponent"] = animationObject;
    }
    if (auto *script = m_scene.reg.get<ScriptComponent>(e)) {
      QJsonObject scriptObject;
      scriptObject["scriptPath"] = script->scriptPath.c_str();
      scriptObject["startFunction"] = script->startFunction.c_str();
      scriptObject["updateFunction"] = script->updateFunction.c_str();
      scriptObject["destroyFunction"] = script->destroyFunction.c_str();
      entityObject["ScriptComponent"] = scriptObject;
    }
    if (m_scene.reg.has<SceneBackgroundComponent>(e)) {
      entityObject["SceneBackgroundComponent"] = true;
    }
    if (auto *shape = m_scene.reg.get<ShapeComponent>(e)) {
      QJsonObject shapeObject;
      shapeObject["kind"] = (int)shape->kind;
      shapeObject["properties"] = shape->getProperties().toJsonObject();
      entityObject["ShapeComponent"] = shapeObject;
    }
    m_entitiesData.append(entityObject);
  }
}

void DeleteCommand::undo() {
  auto &m_scene = m_mainWindow->canvas()->scene();
  m_entities.clear(); // Clear existing entities as they will be recreated
  for (const QJsonObject &entityData : m_entitiesData) {
    Entity newEntity = m_scene.reg.create();
    if (entityData.contains("NameComponent") &&
        entityData["NameComponent"].isString()) {
      m_scene.reg.emplace<NameComponent>(
          newEntity,
          NameComponent{entityData["NameComponent"].toString().toStdString()});
    }
    if (entityData.contains("TransformComponent") &&
        entityData["TransformComponent"].isObject()) {
      QJsonObject transformObject = entityData["TransformComponent"].toObject();
      m_scene.reg.emplace<TransformComponent>(
          newEntity,
          TransformComponent{(float)transformObject["x"].toDouble(),
                             (float)transformObject["y"].toDouble(),
                             (float)transformObject["rotation"].toDouble(),
                             (float)transformObject["sx"].toDouble(),
                             (float)transformObject["sy"].toDouble()});
    }
    if (entityData.contains("MaterialComponent") &&
        entityData["MaterialComponent"].isObject()) {
      QJsonObject materialObject = entityData["MaterialComponent"].toObject();
      m_scene.reg.emplace<MaterialComponent>(
          newEntity,
          MaterialComponent{
              (SkColor)materialObject["color"].toVariant().toULongLong(),
              materialObject["isFilled"].toBool(),
              materialObject["isStroked"].toBool(),
              (float)materialObject["strokeWidth"].toDouble(),
              materialObject["antiAliased"].toBool()});
    }
    if (entityData.contains("AnimationComponent") &&
        entityData["AnimationComponent"].isObject()) {
      QJsonObject animationObject = entityData["AnimationComponent"].toObject();
      m_scene.reg.emplace<AnimationComponent>(
          newEntity,
          AnimationComponent{(float)animationObject["entryTime"].toDouble(),
                             (float)animationObject["exitTime"].toDouble()});
    }
    if (entityData.contains("ScriptComponent") &&
        entityData["ScriptComponent"].isObject()) {
      QJsonObject scriptObject = entityData["ScriptComponent"].toObject();
      m_scene.reg.emplace<ScriptComponent>(
          newEntity,
          ScriptComponent{
              scriptObject["scriptPath"].toString().toStdString(),
              scriptObject["startFunction"].toString().toStdString(),
              scriptObject["updateFunction"].toString().toStdString(),
              scriptObject["destroyFunction"].toString().toStdString()});
    }
    if (entityData.contains("SceneBackgroundComponent") &&
        entityData["SceneBackgroundComponent"].toBool()) {
      m_scene.reg.emplace<SceneBackgroundComponent>(newEntity);
    }
    if (entityData.contains("ShapeComponent") &&
        entityData["ShapeComponent"].isObject()) {
      QJsonObject shapeObject = entityData["ShapeComponent"].toObject();
      ShapeComponent::Kind kind =
          (ShapeComponent::Kind)shapeObject["kind"].toInt();
      ShapeComponent shapeComp{kind, std::monostate{}};

      if (shapeObject.contains("properties") &&
          shapeObject["properties"].isObject()) {
        QJsonObject propsObject = shapeObject["properties"].toObject();
        switch (kind) {
        case ShapeComponent::Kind::Rectangle: {
          RectangleProperties props;
          const QMetaObject &metaObject = props.staticMetaObject;
          for (int i = metaObject.propertyOffset();
               i < metaObject.propertyCount(); ++i) {
            QMetaProperty metaProperty = metaObject.property(i);
            if (propsObject.contains(metaProperty.name())) {
              metaProperty.writeOnGadget(
                  &props, propsObject[metaProperty.name()].toVariant());
            }
          }
          shapeComp.properties = props;
          break;
        }
        case ShapeComponent::Kind::Circle: {
          CircleProperties props;
          const QMetaObject &metaObject = props.staticMetaObject;
          for (int i = metaObject.propertyOffset();
               i < metaObject.propertyCount(); ++i) {
            QMetaProperty metaProperty = metaObject.property(i);
            if (propsObject.contains(metaProperty.name())) {
              metaProperty.writeOnGadget(
                  &props, propsObject[metaProperty.name()].toVariant());
            }
          }
          shapeComp.properties = props;
          break;
        }
        }
      } else {
        // If properties are empty or missing, initialize with defaults
        // based on kind
        if (kind == ShapeComponent::Kind::Rectangle) {
          shapeComp.properties.emplace<RectangleProperties>();
        } else if (kind == ShapeComponent::Kind::Circle) {
          shapeComp.properties.emplace<CircleProperties>();
        }
      }
      m_scene.reg.emplace<ShapeComponent>(newEntity, shapeComp);
    }
    m_entities.append(newEntity); // Add the newly created entity to the list
  }
  m_mainWindow->sceneModel()->refresh();
  m_mainWindow->canvas()->update();
}

void DeleteCommand::redo() {
  auto &m_scene = m_mainWindow->canvas()->scene();
  for (Entity e : m_entities) {
    m_scene.reg.destroy(e);
  }
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
