
#pragma once

#include "ecs.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonObject>
#include <QWidget>

#include "include/core/SkCanvas.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

#include <cmath>
#include <functional>
#include <memory>

class MainWindow;

//==============================================================================
// Shape Base Class (Interface)
//==============================================================================
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

    canvas->drawPath(getPath(), paint);
  }

  SkRect getBoundingBox() const { return getPath().getBounds(); }

  virtual const char *getKindName() const = 0;
  virtual QWidget *
  createPropertyEditor(QWidget *parent,
                       std::function<void(QJsonObject)> onChange) = 0;
  virtual QJsonObject serialize() const = 0;
  virtual void deserialize(const QJsonObject &props) = 0;
  virtual std::unique_ptr<Shape> clone() const = 0;

protected:
  virtual void rebuildPath() const = 0;

  void addNumericProperty(QFormLayout *layout, QWidget *parent,
                          const char *label, double value,
                          std::function<void(double)> onValueChanged) {
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(-10000, 10000);
    spinBox->setDecimals(2);
    spinBox->setValue(value);
    QObject::connect(spinBox,
                     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     parent, onValueChanged);
    layout->addRow(label, spinBox);
  }

  const SkPath &getPath() const {
    if (m_isDirty) {
      rebuildPath();
      m_isDirty = false;
    }
    return m_path;
  }

  mutable SkPath m_path;
  mutable bool m_isDirty = true;
};

//==============================================================================
// Macros for Code Generation
//==============================================================================
#define DECLARE_PROPERTY(type, name, json_name, default_value)                 \
  type name = default_value;
#define SERIALIZE_PROPERTY(type, name, json_name, default_value)               \
  props[json_name] = name;

#define DESERIALIZE_PROPERTY(type, name, json_name, default_value)             \
  name = props.value(json_name).toDouble(default_value);
#define EDITOR_PROPERTY(type, name, json_name, default_value)                  \
  addNumericProperty(form, parent, json_name, name,                            \
                     [this, onChange](double v) {                              \
                       this->name = v;                                         \
                       this->markDirty();                                      \
                       if (onChange)                                           \
                         onChange(this->serialize());                          \
                     });

#define DEFINE_SHAPE_CLASS(ClassName, KindNameString, PROPERTIES_MACRO)        \
  class ClassName : public Shape {                                             \
  public:                                                                      \
    ClassName() { markDirty(); } /* Ensure path is built on creation */        \
    const char *getKindName() const override { return KindNameString; }        \
    std::unique_ptr<Shape> clone() const override {                            \
      return std::make_unique<ClassName>(*this);                               \
    }                                                                          \
    QWidget *                                                                  \
    createPropertyEditor(QWidget *parent,                                      \
                         std::function<void(QJsonObject)> onChange) override { \
      auto *form = new QFormLayout();                                          \
      PROPERTIES_MACRO(EDITOR_PROPERTY)                                        \
      auto *box = new QWidget(parent);                                         \
      box->setLayout(form);                                                    \
      return box;                                                              \
    }                                                                          \
    QJsonObject serialize() const override {                                   \
      QJsonObject props;                                                       \
      PROPERTIES_MACRO(SERIALIZE_PROPERTY)                                     \
      return props;                                                            \
    }                                                                          \
    void deserialize(const QJsonObject &props) override {                      \
      PROPERTIES_MACRO(DESERIALIZE_PROPERTY)                                   \
      markDirty();                                                             \
    }                                                                          \
                                                                               \
  protected:                                                                   \
    void rebuildPath() const override;                                         \
    void markDirty() { m_isDirty = true; }                                     \
                                                                               \
  private:                                                                     \
    PROPERTIES_MACRO(DECLARE_PROPERTY)                                         \
  };
#define RECTANGLE_PROPERTIES(P)                                                \
  P(float, width, "Width", 100.0f)                                             \
  P(float, height, "Height", 60.0f)
DEFINE_SHAPE_CLASS(RectangleShape, "Rectangle", RECTANGLE_PROPERTIES)

#define CIRCLE_PROPERTIES(P) P(float, radius, "Radius", 50.0f)
DEFINE_SHAPE_CLASS(CircleShape, "Circle", CIRCLE_PROPERTIES)

#define REGULAR_POLYGRAM_PROPERTIES(P)                                         \
  P(int, num_vertices, "Num Vertices", 5)                                      \
  P(float, radius, "Radius", 50.0f)                                            \
  P(int, density, "Density", 1)                                                \
  P(float, start_angle, "Start Angle", 0.0f)
DEFINE_SHAPE_CLASS(RegularPolygramShape, "RegularPolygram",
                   REGULAR_POLYGRAM_PROPERTIES)

namespace ShapeFactory {
inline std::unique_ptr<Shape> create(const std::string &kind) {
  if (kind == "Rectangle")
    return std::make_unique<RectangleShape>();
  if (kind == "Circle")
    return std::make_unique<CircleShape>();
  if (kind == "RegularPolygram")
    return std::make_unique<RegularPolygramShape>();
  return nullptr; // Or return a default shape
}
} // namespace ShapeFactory
