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

    addTool("Selection", QIcon(":/icons/cursor.svg"), "tool/select");
    addTool("Rectangle", QIcon(":/icons/rect.svg"), "shape/rect");
    addTool("Circle", QIcon(":/icons/circle.svg"), "shape/circle");
    addTool("Line", QIcon(":/icons/line.svg"), "shape/line");
    addTool("Bezier", QIcon(":/icons/bezier.svg"), "shape/bezier");
    addTool("Text", QIcon(":/icons/text.svg"), "shape/text");
    addTool("Image", QIcon(":/icons/image.svg"), "shape/image");
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
