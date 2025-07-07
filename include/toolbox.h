#pragma once
#include <QDrag>
#include <QListWidget>
#include <QMimeData>

class ToolboxWidget : public QListWidget {
public:
  explicit ToolboxWidget(QWidget *parent = nullptr) : QListWidget(parent) {
    setViewMode(QListView::IconMode);
    setIconSize({32, 32});
    setResizeMode(QListView::Adjust);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);

    addTool("Rectangle", QIcon(":/icons/rect.svg"), "Rectangle");
    addTool("Circle", QIcon(":/icons/circle.svg"), "Circle");
    addTool("Regular Polygram", QIcon(":/icons/circle.svg"), "RegularPolygram");
    addTool("Line", QIcon(":/icons/line.svg"), "Line");
    addTool("Arc", QIcon(":/icons/arc.svg"), "Arc");
    addTool("Arc Between Points", QIcon(":/icons/arc_between_points.svg"), "ArcBetweenPoints");
    addTool("Curved Arrow", QIcon(":/icons/curved_arrow.svg"), "CurvedArrow");
    addTool("Curved Double Arrow", QIcon(":/icons/curved_double_arrow.svg"), "CurvedDoubleArrow");
    addTool("Annular Sector", QIcon(":/icons/arc.svg"), "AnnularSector");
    addTool("Sector", QIcon(":/icons/arc.svg"), "Sector");
    addTool("Annulus", QIcon(":/icons/circle.svg"), "Annulus");
  }

protected:
  QMimeData *mimeData(const QList<QListWidgetItem *> items) const override {
    auto *mime = new QMimeData;
    if (!items.isEmpty())
      mime->setData("application/x-skia-shape",
                    items.first()->data(Qt::UserRole).toByteArray());
    return mime;
  }

private:
  void addTool(const QString &label, const QIcon &icon, const QByteArray &id) {
    auto *it = new QListWidgetItem(icon, label, this);
    it->setData(Qt::UserRole, id); // store the shape id
  }
};
