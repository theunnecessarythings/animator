#pragma once
// SceneModel – a flat QAbstractItemModel that exposes all ECS entities
// so the Scene dock (QTreeView) can list them.  One row per entity.
//
//   • Uses the Registry inside your Scene (ecs.h).
//   • Call refresh() whenever entities change (e.g. after a drop).
//   • Minimal now: shows "Entity <id>"; later add NameComponent, icons, etc.
//
#include "ecs.h"
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
      return createIndex(row, column, nullptr);
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
    ShapePropertiesRole = Qt::UserRole + 1
  };

  QVariant data(const QModelIndex &idx,
                int role = Qt::DisplayRole) const override {
    if (!idx.isValid())
      return {};
    Entity e = entities_[idx.row()];
    if (role == Qt::DisplayRole) {
      if (auto *name = scene_->reg.get<NameComponent>(e)) {
        return QString::fromStdString(name->name);
      }
      return QString("Entity %1").arg(e);
    } else if (role == ShapePropertiesRole) {
      if (auto *shape = scene_->reg.get<ShapeComponent>(e)) {
        QVariantMap properties;
        if (shape->kind == ShapeComponent::Kind::Rectangle) {
          if (auto *rectProps = std::get_if<RectangleProperties>(&shape->properties)) {
            properties["width"] = rectProps->width;
            properties["height"] = rectProps->height;
            properties["grid_xstep"] = rectProps->grid_xstep;
            properties["grid_ystep"] = rectProps->grid_ystep;
          }
        } else if (shape->kind == ShapeComponent::Kind::Circle) {
          if (auto *circleProps = std::get_if<CircleProperties>(&shape->properties)) {
            properties["radius"] = circleProps->radius;
          }
        }
        return properties;
      }
    }
    return {};
  }

  Entity getEntity(const QModelIndex &idx) const {
    if (!idx.isValid())
      return kInvalidEntity;
    return entities_[idx.row()];
  }

  QModelIndex indexOfEntity(Entity entity) const {
    auto it = std::find(entities_.begin(), entities_.end(), entity);
    if (it != entities_.end()) {
      return createIndex(std::distance(entities_.begin(), it), 0);
    }
    return QModelIndex();
  }

  // ---------------------------------------------------------------------
  //  Public helper to rebuild the list when the scene changes
  // ---------------------------------------------------------------------
  void refresh() {
    entities_.clear();
    scene_->reg.each<TransformComponent>(
        [&](Entity e, auto &) { entities_.push_back(e); });
    beginResetModel();
    endResetModel();
  }

private:
  Scene *scene_ = nullptr; // non‑owning
  std::vector<Entity> entities_;
};
