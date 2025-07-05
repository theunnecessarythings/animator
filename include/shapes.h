#pragma once

#include "ecs.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonObject>
#include <QWidget>

#include "include/core/SkCanvas.h"
#include "include/core/SkRect.h"

#include <functional>
#include <memory>

class MainWindow;

class Shape {
public:
  virtual ~Shape() = default;

  void render(SkCanvas *canvas, const MaterialComponent &material) const {
    SkPaint paint;
    paint.setAntiAlias(material.antiAliased);
    paint.setColor(material.color);
    if (material.isFilled && material.isStroked) {
      paint.setStyle(SkPaint::kStrokeAndFill_Style);
    } else if (material.isFilled) {
      paint.setStyle(SkPaint::kFill_Style);
    } else if (material.isStroked) {
      paint.setStyle(SkPaint::kStroke_Style);
    }
    paint.setStrokeWidth(material.strokeWidth);

    drawShape(canvas, paint);
  }

  virtual const char *getKindName() const = 0;
  virtual QWidget *
  createPropertyEditor(QWidget *parent,
                       std::function<void(QJsonObject)> onChange) = 0;
  virtual QJsonObject serialize() const = 0;
  virtual void deserialize(const QJsonObject &props) = 0;
  virtual std::unique_ptr<Shape> clone() const = 0;
  virtual SkRect getBoundingBox() const = 0;

protected:
  virtual void drawShape(SkCanvas *canvas, const SkPaint &paint) const = 0;
  void addNumericProperty(QFormLayout *layout, QWidget *parent,
                          const char *label, double value,
                          std::function<void(double)> onValueChanged) {
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(0, 10000);
    spinBox->setDecimals(2);
    spinBox->setValue(value);
    QObject::connect(spinBox,
                     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     parent, onValueChanged);
    layout->addRow(label, spinBox);
  }
};

class RectangleShape : public Shape {
public:
  const char *getKindName() const override { return "Rectangle"; }

  QWidget *
  createPropertyEditor(QWidget *parent,
                       std::function<void(QJsonObject)> onChange) override {
    auto *form = new QFormLayout();
    addNumericProperty(form, parent, "Width", width,
                       [this, onChange](double v) {
                         this->width = v;
                         if (onChange)
                           onChange(this->serialize());
                       });
    addNumericProperty(form, parent, "Height", height,
                       [this, onChange](double v) {
                         this->height = v;
                         if (onChange)
                           onChange(this->serialize());
                       });

    auto *box = new QWidget(parent);
    box->setLayout(form);
    return box;
  }

  QJsonObject serialize() const override {
    QJsonObject props;
    props["width"] = width;
    props["height"] = height;
    return props;
  }

  void deserialize(const QJsonObject &props) override {
    width = props.value("width").toDouble(100.0);
    height = props.value("height").toDouble(60.0);
  }

  std::unique_ptr<Shape> clone() const override {
    return std::make_unique<RectangleShape>(*this);
  }

  SkRect getBoundingBox() const override {
    return SkRect::MakeWH(width, height);
  }

protected:
  void drawShape(SkCanvas *canvas, const SkPaint &paint) const override {
    canvas->drawRect(SkRect::MakeWH(width, height), paint);
  }

private:
  float width = 100.0f;
  float height = 60.0f;
};

class CircleShape : public Shape {
public:
  const char *getKindName() const override { return "Circle"; }

  QWidget *
  createPropertyEditor(QWidget *parent,
                       std::function<void(QJsonObject)> onChange) override {
    auto *form = new QFormLayout();
    addNumericProperty(form, parent, "Radius", radius,
                       [this, onChange](double v) {
                         this->radius = v;
                         if (onChange)
                           onChange(this->serialize());
                       });

    auto *box = new QWidget(parent);
    box->setLayout(form);
    return box;
  }

  QJsonObject serialize() const override {
    QJsonObject props;
    props["radius"] = radius;
    return props;
  }

  void deserialize(const QJsonObject &props) override {
    radius = props.value("radius").toDouble(50.0);
  }

  std::unique_ptr<Shape> clone() const override {
    return std::make_unique<CircleShape>(*this);
  }

  SkRect getBoundingBox() const override {
    return SkRect::MakeLTRB(-radius, -radius, radius, radius);
  }

protected:
  void drawShape(SkCanvas *canvas, const SkPaint &paint) const override {
    canvas->drawCircle(0, 0, radius, paint);
  }

private:
  float radius = 50.0f;
};

namespace ShapeFactory {
inline std::unique_ptr<Shape> create(const std::string &kind) {
  if (kind == "Rectangle")
    return std::make_unique<RectangleShape>();
  if (kind == "Circle")
    return std::make_unique<CircleShape>();
  return nullptr; // Or return a default shape
}
} // namespace ShapeFactory
