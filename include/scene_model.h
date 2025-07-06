// #pragma once
// //
// // SceneModel – a flat QAbstractItemModel exposing every entity in the Scene.
// // One row per entity.  Call refresh() whenever the Scene changes.
// //
// // Columns: [0] display name ("NameComponent" or "Entity <id>")
// //
// #include "ecs.h"
// #include <QAbstractItemModel>
// #include <vector>
//
// class SceneModel : public QAbstractItemModel {
//   Q_OBJECT
// public:
//   explicit SceneModel(Scene *scene, QObject *parent = nullptr)
//       : QAbstractItemModel(parent), scene_(scene) {
//     refresh();
//   }
//
//   // ---------------------------------------------------------------------
//   //  QAbstractItemModel overrides
//   // ---------------------------------------------------------------------
//   QModelIndex index(int row, int column,
//                     const QModelIndex &parent = QModelIndex()) const override
//                     {
//     if (!parent.isValid() && row >= 0 &&
//         row < static_cast<int>(entities_.size()))
//       return createIndex(row, column);
//     return {};
//   }
//
//   QModelIndex parent(const QModelIndex &) const override { return {}; }
//
//   int rowCount(const QModelIndex &parent = QModelIndex()) const override {
//     return parent.isValid() ? 0 : static_cast<int>(entities_.size());
//   }
//
//   int columnCount(const QModelIndex & = QModelIndex()) const override {
//     return 1;
//   }
//
//   enum SceneModelRoles {
//     ShapePropertiesRole = Qt::UserRole + 1,
//     ScriptPropertiesRole = Qt::UserRole + 2
//   };
//
//   QVariant data(const QModelIndex &idx,
//                 int role = Qt::DisplayRole) const override {
//     if (!idx.isValid())
//       return {};
//
//     Entity e = entities_[idx.row()];
//
//     switch (role) {
//     case Qt::DisplayRole:
//       if (auto *n = scene_->reg.get<NameComponent>(e))
//         return QString::fromStdString(n->name);
//       return QStringLiteral("Entity %1").arg(static_cast<qulonglong>(e));
//
//     case ShapePropertiesRole:
//       if (auto *sc = scene_->reg.get<ShapeComponent>(e))
//         if (sc->shape)
//           return sc->shape->serialize();
//       break;
//
//     case ScriptPropertiesRole:
//       if (auto *scr = scene_->reg.get<ScriptComponent>(e)) {
//         QVariantMap m;
//         m["scriptPath"] = QString::fromStdString(scr->scriptPath);
//         m["startFunction"] = QString::fromStdString(scr->startFunction);
//         m["updateFunction"] = QString::fromStdString(scr->updateFunction);
//         m["destroyFunction"] = QString::fromStdString(scr->destroyFunction);
//         return m;
//       }
//       break;
//
//     default:
//       break;
//     }
//     return {};
//   }
//
//   // helper ---------------------------------------------------------------
//   Entity getEntity(const QModelIndex &idx) const {
//     return idx.isValid() ? entities_[idx.row()] : kInvalidEntity;
//   }
//
//   QModelIndex indexOfEntity(Entity e) const {
//     auto it = std::find(entities_.begin(), entities_.end(), e);
//     return it != entities_.end()
//                ? createIndex(
//                      static_cast<int>(std::distance(entities_.begin(), it)),
//                      0)
//                : QModelIndex();
//   }
//
//   // ---------------------------------------------------------------------
//   //  repopulate list after scene edits
//   // ---------------------------------------------------------------------
//   void refresh() {
//     beginResetModel();
//     entities_.clear();
//     scene_->reg.each<TransformComponent>(
//         [&](Entity e, const TransformComponent &) { entities_.push_back(e);
//         });
//     endResetModel();
//   }
//
// private:
//   Scene *scene_ = nullptr; // non-owning
//   std::vector<Entity> entities_;
// };
//

#pragma once

#include "scene.h"
#include <QAbstractItemModel>
#include <vector>

class SceneModel : public QAbstractItemModel {
  Q_OBJECT
public:
  explicit SceneModel(Scene *scene, QObject *parent = nullptr)
      : QAbstractItemModel(parent), scene_(scene) {
    refresh();
  }

  // ---------------------------------------------------------------------
  //  QAbstractItemModel overrides
  // ---------------------------------------------------------------------
  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override {
    if (!parent.isValid() && row >= 0 &&
        row < static_cast<int>(entities_.size()))
      return createIndex(row, column);
    return {};
  }
  QModelIndex parent(const QModelIndex &) const override { return {}; }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(entities_.size());
  }
  int columnCount(const QModelIndex & = QModelIndex()) const override {
    return 1;
  }

  enum SceneModelRoles {
    ShapePropertiesRole = Qt::UserRole + 1,
    ScriptPropertiesRole = Qt::UserRole + 2
  };

  QVariant data(const QModelIndex &idx,
                int role = Qt::DisplayRole) const override {
    if (!idx.isValid())
      return {};
    Entity e = entities_[idx.row()];
    switch (role) {
    case Qt::DisplayRole:
      if (e.has<NameComponent>()) {
        auto n = e.get<NameComponent>();
        return QString::fromStdString(n.name);
      }
      return QStringLiteral("Entity %1").arg(static_cast<qulonglong>(e.id()));

    case ShapePropertiesRole:
      if (e.has<ShapeComponent>()) {
        auto sc = e.get<ShapeComponent>();
        if (sc.shape)
          return sc.shape->serialize();
      }
      break;

    case ScriptPropertiesRole:
      if (e.has<ScriptComponent>()) {
        auto scr = e.get<ScriptComponent>();
        QVariantMap m;
        m["scriptPath"] = QString::fromStdString(scr.scriptPath);
        m["startFunction"] = QString::fromStdString(scr.startFunction);
        m["updateFunction"] = QString::fromStdString(scr.updateFunction);
        m["destroyFunction"] = QString::fromStdString(scr.destroyFunction);
        return m;
      }
      break;
    default:
      break;
    }
    return {};
  }

  // Helpers -------------------------------------------------------------
  Entity getEntity(const QModelIndex &idx) const {
    return idx.isValid() ? entities_[idx.row()] : kInvalidEntity;
  }
  QModelIndex indexOfEntity(Entity e) const {
    auto it = std::find(entities_.begin(), entities_.end(), e);
    return it != entities_.end()
               ? createIndex(
                     static_cast<int>(std::distance(entities_.begin(), it)), 0)
               : QModelIndex();
  }

  // ---------------------------------------------------------------------
  //  Repopulate list after scene edits
  // ---------------------------------------------------------------------
  void refresh() {
    beginResetModel();
    entities_.clear();
    scene_->ecs().each<TransformComponent>(
        [&](flecs::entity ent, TransformComponent &) {
          entities_.push_back(ent);
        });
    endResetModel();
  }

private:
  Scene *scene_ = nullptr;       // non-owning
  std::vector<Entity> entities_; // row cache
};
