#pragma once

#include "ecs.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUndoCommand>

struct Scene;     // fwd
class MainWindow; // fwd

// ──────────────────────────────────────────────────────────────────────────────
//  Add / Remove single entity
// ──────────────────────────────────────────────────────────────────────────────
class AddEntityCommand : public QUndoCommand {
public:
  AddEntityCommand(MainWindow *window, Entity entity,
                   QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_entityData;
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
  QJsonObject m_entityData;
};

// ──────────────────────────────────────────────────────────────────────────────
//  Transform & movement
// ──────────────────────────────────────────────────────────────────────────────
class MoveEntityCommand : public QUndoCommand {
public:
  MoveEntityCommand(MainWindow *window, Entity entity, float oldX, float oldY,
                    float oldRot, float newX, float newY, float newRot,
                    QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldX, m_oldY, m_oldRot;
  float m_newX, m_newY, m_newRot;
};

// ──────────────────────────────────────────────────────────────────────────────
//  Cut / Delete multi-selection
// ──────────────────────────────────────────────────────────────────────────────
class CutCommand : public QUndoCommand {
public:
  CutCommand(MainWindow *window, const QList<Entity> &entities,
             QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  QList<Entity> m_entities;
  QList<QJsonObject> m_entitiesData;
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
  QList<QJsonObject> m_entitiesData;
};

// ──────────────────────────────────────────────────────────────────────────────
//  Property-change commands
// ──────────────────────────────────────────────────────────────────────────────
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
  std::string m_oldName, m_newName;
};

class ChangeTransformCommand : public QUndoCommand {
public:
  enum { Id = 1234 };

  ChangeTransformCommand(MainWindow *window, Entity entity, float oldX,
                         float oldY, float oldRot, float oldSx, float oldSy,
                         float newX, float newY, float newRot, float newSx,
                         float newSy, QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

  int id() const override { return Id; }
  bool mergeWith(const QUndoCommand *other) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldX, m_oldY, m_oldRot, m_oldSx, m_oldSy;
  float m_newX, m_newY, m_newRot, m_newSx, m_newSy;
};

class ChangeMaterialCommand : public QUndoCommand {
public:
  ChangeMaterialCommand(MainWindow *window, Entity entity, SkColor oldColor,
                        bool oldIsFilled, bool oldIsStroked,
                        float oldStrokeWidth, bool oldAA, SkColor newColor,
                        bool newIsFilled, bool newIsStroked,
                        float newStrokeWidth, bool newAA,
                        QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  SkColor m_oldColor;
  bool m_oldIsFilled, m_oldIsStroked;
  float m_oldStrokeWidth;
  bool m_oldAA;
  SkColor m_newColor;
  bool m_newIsFilled, m_newIsStroked;
  float m_newStrokeWidth;
  bool m_newAA;
};

class ChangeAnimationCommand : public QUndoCommand {
public:
  ChangeAnimationCommand(MainWindow *window, Entity entity, float oldEntry,
                         float oldExit, float newEntry, float newExit,
                         QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldEntry, m_oldExit;
  float m_newEntry, m_newExit;
};

class ChangeScriptCommand : public QUndoCommand {
public:
  ChangeScriptCommand(MainWindow *window, Entity entity,
                      const std::string &oldPath, const std::string &oldStart,
                      const std::string &oldUpdate,
                      const std::string &oldDestroy, const std::string &newPath,
                      const std::string &newStart, const std::string &newUpdate,
                      const std::string &newDestroy,
                      QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  std::string m_oldPath, m_oldStart, m_oldUpdate, m_oldDestroy;
  std::string m_newPath, m_newStart, m_newUpdate, m_newDestroy;
};

class ChangeShapePropertyCommand : public QUndoCommand {
public:
  ChangeShapePropertyCommand(MainWindow *window, Entity entity,
                             const QJsonObject &oldProps,
                             const QJsonObject &newProps,
                             QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_oldProps, m_newProps;
};
