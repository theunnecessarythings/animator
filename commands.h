#pragma once

#include "ecs.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUndoCommand>

// Forward declaration
class Scene;
class MainWindow;

class AddEntityCommand : public QUndoCommand {
public:
  AddEntityCommand(MainWindow *window, Entity entity,
                   QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_entityData; // To store serialized entity data for undo/redo
  bool m_firstRedo{true};
};

class RemoveEntityCommand : public QUndoCommand {
public:
  RemoveEntityCommand(MainWindow *window, Entity entity,
                      QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_entityData; // To store serialized entity data for undo/redo
};

class MoveEntityCommand : public QUndoCommand {
public:
  MoveEntityCommand(MainWindow *window, Entity entity, float oldX, float oldY,
                    float oldRotation, float newX, float newY,
                    float newRotation, QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldX, m_oldY, m_oldRotation;
  float m_newX, m_newY, m_newRotation;
};

class CutCommand : public QUndoCommand {
public:
  CutCommand(MainWindow *window, const QList<Entity> &entities,
             QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  QList<Entity> m_entities;
  QList<QJsonObject>
      m_entitiesData; // To store serialized entity data for undo/redo
};

class DeleteCommand : public QUndoCommand {
public:
  DeleteCommand(MainWindow *window, const QList<Entity> &entities,
                QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  QList<Entity> m_entities;
  QList<QJsonObject>
      m_entitiesData; // To store serialized entity data for undo/redo
};

class ChangeNameCommand : public QUndoCommand {
public:
  ChangeNameCommand(MainWindow *window, Entity entity,
                    const std::string &oldName, const std::string &newName,
                    QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  std::string m_oldName;
  std::string m_newName;
};

class ChangeTransformCommand : public QUndoCommand {
public:
  ChangeTransformCommand(MainWindow *window, Entity entity, float oldX,
                         float oldY, float oldRotation, float oldSx,
                         float oldSy, float newX, float newY, float newRotation,
                         float newSx, float newSy,
                         QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldX, m_oldY, m_oldRotation, m_oldSx, m_oldSy;
  float m_newX, m_newY, m_newRotation, m_newSx, m_newSy;
};

class ChangeMaterialCommand : public QUndoCommand {
public:
  ChangeMaterialCommand(MainWindow *window, Entity entity, SkColor oldColor,
                        bool oldIsFilled, bool oldIsStroked,
                        float oldStrokeWidth, bool oldAntiAliased,
                        SkColor newColor, bool newIsFilled, bool newIsStroked,
                        float newStrokeWidth, bool newAntiAliased,
                        QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  SkColor m_oldColor;
  bool m_oldIsFilled;
  bool m_oldIsStroked;
  float m_oldStrokeWidth;
  bool m_oldAntiAliased;
  SkColor m_newColor;
  bool m_newIsFilled;
  bool m_newIsStroked;
  float m_newStrokeWidth;
  bool m_newAntiAliased;
};

class ChangeAnimationCommand : public QUndoCommand {
public:
  ChangeAnimationCommand(MainWindow *window, Entity entity, float oldEntryTime,
                         float oldExitTime, float newEntryTime,
                         float newExitTime, QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldEntryTime, m_oldExitTime;
  float m_newEntryTime, m_newExitTime;
};

class ChangeScriptCommand : public QUndoCommand {
public:
  ChangeScriptCommand(
      MainWindow *window, Entity entity, const std::string &oldScriptPath,
      const std::string &oldStartFunction, const std::string &oldUpdateFunction,
      const std::string &oldDestroyFunction, const std::string &newScriptPath,
      const std::string &newStartFunction, const std::string &newUpdateFunction,
      const std::string &newDestroyFunction, QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  std::string m_oldScriptPath, m_oldStartFunction, m_oldUpdateFunction,
      m_oldDestroyFunction;
  std::string m_newScriptPath, m_newStartFunction, m_newUpdateFunction,
      m_newDestroyFunction;
};

class ChangeShapePropertyCommand : public QUndoCommand {
public:
  ChangeShapePropertyCommand(MainWindow *window, Entity entity,
                             ShapeComponent::Kind kind,
                             const QJsonObject &oldProperties,
                             const QJsonObject &newProperties,
                             QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  ShapeComponent::Kind m_kind;
  QJsonObject m_oldProperties;
  QJsonObject m_newProperties;
};
