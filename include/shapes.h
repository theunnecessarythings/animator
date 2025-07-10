#pragma once

// Forward declarations to break circular dependency with ecs.h
struct MaterialComponent;
struct PathEffectComponent;

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "include/core/SkCanvas.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
// Shape Base Class (Interface)
//==============================================================================

// Defines how a path should be styled (filled, stroked, or both)
enum class PathStyle { kFill, kStroke, kStrokeAndFill };

// A simple struct to pair a path with its intended style
struct StyledPath {
  SkPath path;
  std::optional<PathStyle> style = std::nullopt;
};

class Shape {
public:
  virtual ~Shape() = default;

  void render(SkCanvas *canvas, const MaterialComponent &material,
              const PathEffectComponent *pathEffect = nullptr) const;

  SkRect getBoundingBox() const {
    if (m_isDirty) {
      rebuildPaths();
      m_isDirty = false;
    }
    SkRect bounds =
        m_paths.empty() ? SkRect::MakeEmpty() : m_paths[0].path.getBounds();
    for (size_t i = 1; i < m_paths.size(); ++i) {
      bounds.join(m_paths[i].path.getBounds());
    }
    return bounds;
  }

  virtual const char *getKindName() const = 0;
  virtual QWidget *
  createPropertyEditor(QWidget *parent,
                       std::function<void(QJsonObject)> onChange) = 0;
  virtual QJsonObject serialize() const = 0;
  virtual void deserialize(const QJsonObject &props) = 0;
  virtual std::unique_ptr<Shape> clone() const = 0;
  virtual void rebuildPaths() const = 0;

protected:
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
      rebuildPaths();
      m_isDirty = false;
    }
    // Return the first path for simple compatibility, though this might not
    // always be what's needed.
    return m_paths.front().path;
  }

  mutable std::vector<StyledPath> m_paths;
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
    void rebuildPaths() const override;                                        \
                                                                               \
  protected:                                                                   \
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

#define LINE_PROPERTIES(P)                                                     \
  P(float, x1, "X1", 0.0f)                                                     \
  P(float, y1, "Y1", 0.0f)                                                     \
  P(float, x2, "X2", 100.0f)                                                   \
  P(float, y2, "Y2", 0.0f)
DEFINE_SHAPE_CLASS(LineShape, "Line", LINE_PROPERTIES)

#define ARC_PROPERTIES(P)                                                      \
  P(float, radius, "Radius", 50.0f)                                            \
  P(float, start_angle, "Start Angle", 0.0f)                                   \
  P(float, angle, "Angle", 90.0f)                                              \
  P(int, num_components, "Num Components", 16)                                 \
  P(float, arc_center_x, "Center X", 0.0f)                                     \
  P(float, arc_center_y, "Center Y", 0.0f)
DEFINE_SHAPE_CLASS(ArcShape, "Arc", ARC_PROPERTIES)

#define ARC_BETWEEN_POINTS_PROPERTIES(P)                                       \
  P(float, x1, "X1", -50.0f)                                                   \
  P(float, y1, "Y1", 0.0f)                                                     \
  P(float, x2, "X2", 50.0f)                                                    \
  P(float, y2, "Y2", 0.0f)                                                     \
  P(float, angle, "Angle", 90.0f)                                              \
  P(float, radius, "Radius", 0.0f) /* 0.0 means auto-calculate */
DEFINE_SHAPE_CLASS(ArcBetweenPointsShape, "ArcBetweenPoints",
                   ARC_BETWEEN_POINTS_PROPERTIES)

#define CURVED_ARROW_PROPERTIES(P)                                             \
  P(float, x1, "X1", -50.0f)                                                   \
  P(float, y1, "Y1", 0.0f)                                                     \
  P(float, x2, "X2", 50.0f)                                                    \
  P(float, y2, "Y2", 0.0f)                                                     \
  P(float, angle, "Angle", 90.0f)                                              \
  P(float, radius, "Radius", 0.0f) /* 0.0 means auto-calculate */              \
  P(float, arrowhead_size, "Arrowhead Size", 10.0f)
DEFINE_SHAPE_CLASS(CurvedArrowShape, "CurvedArrow", CURVED_ARROW_PROPERTIES)

#define CURVED_DOUBLE_ARROW_PROPERTIES(P)                                      \
  P(float, x1, "X1", -50.0f)                                                   \
  P(float, y1, "Y1", 0.0f)                                                     \
  P(float, x2, "X2", 50.0f)                                                    \
  P(float, y2, "Y2", 0.0f)                                                     \
  P(float, angle, "Angle", 90.0f)                                              \
  P(float, radius, "Radius", 0.0f) /* 0.0 means auto-calculate */              \
  P(float, arrowhead_size, "Arrowhead Size", 10.0f)
DEFINE_SHAPE_CLASS(CurvedDoubleArrowShape, "CurvedDoubleArrow",
                   CURVED_DOUBLE_ARROW_PROPERTIES)

#define ANNULAR_SECTOR_PROPERTIES(P)                                           \
  P(float, inner_radius, "Inner Radius", 50.0f)                                \
  P(float, outer_radius, "Outer Radius", 100.0f)                               \
  P(float, start_angle, "Start Angle", 0.0f)                                   \
  P(float, angle, "Angle", 90.0f)                                              \
  P(float, arc_center_x, "Center X", 0.0f)                                     \
  P(float, arc_center_y, "Center Y", 0.0f)
DEFINE_SHAPE_CLASS(AnnularSectorShape, "AnnularSector",
                   ANNULAR_SECTOR_PROPERTIES)

#define SECTOR_PROPERTIES(P)                                                   \
  P(float, radius, "Radius", 100.0f)                                           \
  P(float, start_angle, "Start Angle", 0.0f)                                   \
  P(float, angle, "Angle", 90.0f)                                              \
  P(float, arc_center_x, "Center X", 0.0f)                                     \
  P(float, arc_center_y, "Center Y", 0.0f)
DEFINE_SHAPE_CLASS(SectorShape, "Sector", SECTOR_PROPERTIES)

#define ANNULUS_PROPERTIES(P)                                                  \
  P(float, inner_radius, "Inner Radius", 1.0f)                                 \
  P(float, outer_radius, "Outer Radius", 2.0f)                                 \
  P(float, center_x, "Center X", 0.0f)                                         \
  P(float, center_y, "Center Y", 0.0f)
DEFINE_SHAPE_CLASS(AnnulusShape, "Annulus", ANNULUS_PROPERTIES)

#define CUBIC_BEZIER_PROPERTIES(P)                                             \
  P(float, x1, "Start Anchor X", -100.0f)                                      \
  P(float, y1, "Start Anchor Y", 0.0f)                                         \
  P(float, x2, "Start Handle X", -50.0f)                                       \
  P(float, y2, "Start Handle Y", 50.0f)                                        \
  P(float, x3, "End Handle X", 50.0f)                                          \
  P(float, y3, "End Handle Y", -50.0f)                                         \
  P(float, x4, "End Anchor X", 100.0f)                                         \
  P(float, y4, "End Anchor Y", 0.0f)
DEFINE_SHAPE_CLASS(CubicBezierShape, "CubicBezier", CUBIC_BEZIER_PROPERTIES)

class ArcPolygonShape : public Shape {
public:
  std::vector<SkPoint> vertices;
  std::vector<float> angles;
  std::vector<float> radii;

  ArcPolygonShape() {
    vertices.push_back(SkPoint::Make(-50, 50));
    vertices.push_back(SkPoint::Make(50, 50));
    vertices.push_back(SkPoint::Make(0, -50));
    angles.assign(3, 45.0f);
    radii.assign(3, 0.0f);
    markDirty();
  }
  const char *getKindName() const override { return "ArcPolygon"; }
  std::unique_ptr<Shape> clone() const override {
    return std::make_unique<ArcPolygonShape>(*this);
  }
  QWidget *
  createPropertyEditor(QWidget *parent,
                       std::function<void(QJsonObject)> onChange) override;
  QJsonObject serialize() const override;
  void deserialize(const QJsonObject &props) override;
  void rebuildPaths() const override;

protected:
  void markDirty() { m_isDirty = true; }
};

namespace ShapeFactory {

inline std::unique_ptr<Shape> create(const std::string &kind) {
  if (kind == "Rectangle")
    return std::make_unique<RectangleShape>();
  if (kind == "Circle")
    return std::make_unique<CircleShape>();
  if (kind == "RegularPolygram")
    return std::make_unique<RegularPolygramShape>();
  if (kind == "Line")
    return std::make_unique<LineShape>();
  if (kind == "Arc")
    return std::make_unique<ArcShape>();
  if (kind == "ArcBetweenPoints")
    return std::make_unique<ArcBetweenPointsShape>();
  if (kind == "CurvedArrow")
    return std::make_unique<CurvedArrowShape>();
  if (kind == "CurvedDoubleArrow")
    return std::make_unique<CurvedDoubleArrowShape>();
  if (kind == "AnnularSector")
    return std::make_unique<AnnularSectorShape>();
  if (kind == "Sector")
    return std::make_unique<SectorShape>();
  if (kind == "Annulus")
    return std::make_unique<AnnulusShape>();
  if (kind == "CubicBezier")
    return std::make_unique<CubicBezierShape>();
  if (kind == "ArcPolygon")
    return std::make_unique<ArcPolygonShape>();
  return nullptr;
}
} // namespace ShapeFactory
