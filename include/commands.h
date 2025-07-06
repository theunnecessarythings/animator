#pragma once

#include "ecs.h"
#include "serialization.h"
#include "window.h"
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QUndoCommand>

class Scene;

void applyJsonToEntity(flecs::world &world, flecs::entity e,
                       const QJsonObject &o, bool is_paste);

class SceneCommand : public QUndoCommand {
public:
  using QUndoCommand::QUndoCommand;
  template <typename T> static const char *getComponentJsonKey();
  virtual void updateEntityIds(const QMap<qint64, Entity> &) {}
};

template <typename T> class SetComponentCommand : public SceneCommand {
public:
  SetComponentCommand(MainWindow *window, Entity entity,
                      const QJsonObject &oldData, const QJsonObject &newData,
                      QUndoCommand *parent = nullptr)
      : SceneCommand(parent), m_mainWindow(window), m_entity(entity),
        m_oldData(oldData), m_newData(newData) {
    setText(QObject::tr("Set %1").arg(getComponentJsonKey<T>()));
  }

  void undo() override {
    if (!m_entity.is_alive())
      return;
    // If old data was empty, it means the component didn't exist. So, remove
    // it.
    if (m_oldData.isEmpty()) {
      m_entity.remove<T>();
    } else {
      // Otherwise, restore the old data.
      QJsonObject wrapper{{getComponentJsonKey<T>(), m_oldData}};
      applyJsonToEntity(m_mainWindow->canvas()->scene().ecs(), m_entity,
                        wrapper, false);
    }
    m_mainWindow->refreshProperties();
  }

  void redo() override {
    if (!m_entity.is_alive())
      return;
    if (m_newData.isEmpty()) {
      m_entity.remove<T>();
    } else {
      QJsonObject wrapper{{getComponentJsonKey<T>(), m_newData}};
      applyJsonToEntity(m_mainWindow->canvas()->scene().ecs(), m_entity,
                        wrapper, false);
    }
    m_mainWindow->refreshProperties();
  }

  void updateEntityIds(const QMap<qint64, Entity> &idMap) override {
    if (idMap.contains(m_entity.id())) {
      m_entity = idMap.value(m_entity.id());
    }
  }

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_oldData, m_newData;
};

class AddEntityCommand : public SceneCommand {
public:
  AddEntityCommand(MainWindow *window, Entity entity,
                   QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_entityData;
  bool m_firstRedo{true};
};

class RemoveEntityCommand : public SceneCommand {
public:
  RemoveEntityCommand(MainWindow *window, Entity entity,
                      QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_entityData;
};

class CutCommand : public SceneCommand {
public:
  CutCommand(MainWindow *window, const QList<Entity> &entities,
             QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  QList<Entity> m_entities;
  QList<QJsonObject> m_entitiesData;
};

class DeleteCommand : public SceneCommand {
public:
  DeleteCommand(MainWindow *window, const QList<Entity> &entities,
                QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  QList<Entity> m_entities;
  QList<QJsonObject> m_entitiesData;
};

class MoveEntityCommand : public SceneCommand {
public:
  MoveEntityCommand(MainWindow *window, Entity entity, float oldX, float oldY,
                    float oldRot, float newX, float newY, float newRot,
                    QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldX, m_oldY, m_oldRot;
  float m_newX, m_newY, m_newRot;
};

class ChangeNameCommand : public SceneCommand {
public:
  ChangeNameCommand(MainWindow *window, Entity entity,
                    const std::string &oldName, const std::string &newName,
                    QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  std::string m_oldName, m_newName;
};

class ChangeTransformCommand : public SceneCommand {
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
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldX, m_oldY, m_oldRot, m_oldSx, m_oldSy;
  float m_newX, m_newY, m_newRot, m_newSx, m_newSy;
};

class ChangeMaterialCommand : public SceneCommand {
public:
  ChangeMaterialCommand(MainWindow *window, Entity entity, SkColor oldColor,
                        bool oldFill, bool oldStroke, float oldWidth,
                        bool oldAA, SkColor newColor, bool newFill,
                        bool newStroke, float newWidth, bool newAA,
                        QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  SkColor m_oldColor;
  bool m_oldFill, m_oldStroke;
  float m_oldWidth;
  bool m_oldAA;
  SkColor m_newColor;
  bool m_newFill, m_newStroke;
  float m_newWidth;
  bool m_newAA;
};

class ChangeAnimationCommand : public SceneCommand {
public:
  ChangeAnimationCommand(MainWindow *window, Entity entity, float oldEntry,
                         float oldExit, float newEntry, float newExit,
                         QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  float m_oldEntry, m_oldExit, m_newEntry, m_newExit;
};

class ChangeScriptCommand : public SceneCommand {
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
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  std::string m_oldPath, m_oldStart, m_oldUpdate, m_oldDestroy;
  std::string m_newPath, m_newStart, m_newUpdate, m_newDestroy;
};

class ChangeShapePropertyCommand : public SceneCommand {
public:
  ChangeShapePropertyCommand(MainWindow *window, Entity entity,
                             const QJsonObject &oldProps,
                             const QJsonObject &newProps,
                             QUndoCommand *parent = nullptr);
  void undo() override;
  void redo() override;
  void updateEntityIds(const QMap<qint64, Entity> &idMap) override;

private:
  MainWindow *m_mainWindow;
  Entity m_entity;
  QJsonObject m_oldProps, m_newProps;
};
