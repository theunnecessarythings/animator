#include "shapes.h"
#include "include/core/SkPathMeasure.h"
#include <QJsonArray>

// For simple shapes, we create one path that can be stroked and/or filled.
void RectangleShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;
  styledPath.path.addRect(SkRect::MakeWH(width, height), SkPathDirection::kCW,
                          0);
  m_paths.push_back(styledPath);
}

void CircleShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;
  styledPath.path.addCircle(0, 0, radius, SkPathDirection::kCW);
  m_paths.push_back(styledPath);
}

// Helper for polygram
SkPath createRegularPolygramPath(int num_vertices, float radius, int density,
                                 float start_angle) {
  SkPath path;
  if (num_vertices < 3 || radius <= 0 || density < 1)
    return path;

  float angle_increment = 2 * M_PI / num_vertices;
  float current_angle = start_angle;

  for (int i = 0; i < num_vertices; i += density) {
    float x = radius * cos(current_angle);
    float y = radius * sin(current_angle);
    if (i == 0) {
      path.moveTo(x, y);
    } else {
      path.lineTo(x, y);
    }
    current_angle += angle_increment * density;
  }
  path.close();
  return path;
}

void RegularPolygramShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;
  styledPath.path =
      createRegularPolygramPath(num_vertices, radius, density, start_angle);
  m_paths.push_back(styledPath);
}

// For lines and arcs, we create a single open path.
// The material will determine if it's stroked. Filling will have no effect.
void LineShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;
  styledPath.path.moveTo(x1, y1);
  styledPath.path.lineTo(x2, y2);
  m_paths.push_back(styledPath);
}

void ArcShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;
  styledPath.path.addArc(SkRect::MakeXYWH(arc_center_x - radius,
                                          arc_center_y - radius, 2 * radius,
                                          2 * radius),
                         start_angle, angle);
  m_paths.push_back(styledPath);
}

// Helper function to get parameters for an arc between two points
bool getArcBetweenPointsParams(float x1, float y1, float x2, float y2,
                               float angle, float radius, SkPoint &p1,
                               SkPoint &p2, SkRect &arc_rect,
                               float &start_angle_degrees,
                               float &skia_sweep_angle) {
  p1 = SkPoint::Make(x1, y1);
  p2 = SkPoint::Make(x2, y2);

  float dist = SkPoint::Distance(p1, p2);
  float actual_radius = radius;
  float actual_angle = angle;

  if (actual_radius == 0.0f) {
    if (actual_angle == 0.0f) {
      return false; // Line
    }
    float half_angle_rad = (actual_angle / 2.0f) * M_PI / 180.0f;
    actual_radius = (dist / 2.0f) / sin(half_angle_rad);
  } else {
    float halfdist = dist / 2.0f;
    if (actual_radius < halfdist) {
      return false; // Line
    }
    float cos_half_angle = (actual_radius - sqrt(actual_radius * actual_radius -
                                                 halfdist * halfdist)) /
                           actual_radius;
    actual_angle = acos(cos_half_angle) * 2.0f * 180.0f / M_PI;
  }

  SkPoint mid_point = SkPoint::Make((x1 + x2) / 2.0f, (y1 + y2) / 2.0f);
  float d_mid_to_center_sq =
      actual_radius * actual_radius - (dist * dist / 4.0f);
  if (d_mid_to_center_sq < 0)
    return false; // Line
  float d_mid_to_center = sqrt(d_mid_to_center_sq);

  SkVector vec_p1_to_p2 = p2 - p1;
  SkVector perp_vec = SkVector::Make(-vec_p1_to_p2.fY, vec_p1_to_p2.fX);
  perp_vec.normalize();

  SkPoint center1 = mid_point + perp_vec * d_mid_to_center;
  SkPoint center2 = mid_point - perp_vec * d_mid_to_center;

  SkPoint arc_center;
  float cross_product1 = (p1.fX - center1.fX) * (p2.fY - center1.fY) -
                         (p1.fY - center1.fY) * (p2.fX - center1.fX);

  if ((actual_angle > 0 && cross_product1 > 0) ||
      (actual_angle < 0 && cross_product1 < 0)) {
    arc_center = center1;
  } else {
    arc_center = center2;
  }

  start_angle_degrees =
      atan2(p1.fY - arc_center.fY, p1.fX - arc_center.fX) * 180.0f / M_PI;
  start_angle_degrees = fmod(start_angle_degrees, 360.0f);
  if (start_angle_degrees < 0) {
    start_angle_degrees += 360.0f;
  }

  skia_sweep_angle = -actual_angle;

  arc_rect = SkRect::MakeXYWH(arc_center.fX - actual_radius,
                              arc_center.fY - actual_radius, 2 * actual_radius,
                              2 * actual_radius);
  return true;
}

void ArcBetweenPointsShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;

  SkPoint p1, p2;
  SkRect arc_rect;
  float start_angle_deg, sweep_angle_deg;
  if (getArcBetweenPointsParams(x1, y1, x2, y2, angle, radius, p1, p2, arc_rect,
                                start_angle_deg, sweep_angle_deg)) {
    styledPath.path.addArc(arc_rect, start_angle_deg, sweep_angle_deg);
  } else {
    styledPath.path.moveTo(x1, y1);
    styledPath.path.lineTo(x2, y2);
  }
  m_paths.push_back(styledPath);
}

// Helper function to create an arrowhead path
SkPath createArrowheadPath(const SkPoint &tip_point,
                           const SkVector &direction_vector, float size) {
  SkPath path;
  SkVector dir = direction_vector;
  dir.normalize();
  SkVector perp_dir = SkVector::Make(-dir.fY, dir.fX);

  SkPoint base1 = tip_point - dir * size + perp_dir * (size / 2.0f);
  SkPoint base2 = tip_point - dir * size - perp_dir * (size / 2.0f);

  path.moveTo(tip_point.fX, tip_point.fY);
  path.lineTo(base1.fX, base1.fY);
  path.lineTo(base2.fX, base2.fY);
  path.close();
  return path;
}

void CurvedArrowShape::rebuildPaths() const {
  m_paths.clear();

  // 1. Create the arc path (for stroking)
  StyledPath arcStyledPath;
  SkPath &arcPath = arcStyledPath.path;

  SkPoint p1, p2;
  SkRect arc_rect;
  float start_angle_deg, sweep_angle_deg;
  if (getArcBetweenPointsParams(x1, y1, x2, y2, angle, radius, p1, p2, arc_rect,
                                start_angle_deg, sweep_angle_deg)) {
    arcPath.addArc(arc_rect, start_angle_deg, sweep_angle_deg);
  } else {
    arcPath.moveTo(x1, y1);
    arcPath.lineTo(x2, y2);
  }
  m_paths.push_back(arcStyledPath);

  // 2. Create the arrowhead path (for filling)
  SkPathMeasure path_measure(arcPath, false);
  if (path_measure.getLength() > 0) {
    SkPoint position;
    SkVector tangent;
    if (path_measure.getPosTan(path_measure.getLength(), &position, &tangent)) {
      if (tangent.length() > 1e-6) {
        StyledPath arrowheadStyledPath;
        arrowheadStyledPath.style = PathStyle::kFill;
        arrowheadStyledPath.path =
            createArrowheadPath(position, tangent, arrowhead_size);
        m_paths.push_back(arrowheadStyledPath);
      }
    }
  }
}

void CurvedDoubleArrowShape::rebuildPaths() const {
  m_paths.clear();

  // 1. Create the arc path (for stroking)
  StyledPath arcStyledPath;
  SkPath &arcPath = arcStyledPath.path;

  SkPoint p1, p2;
  SkRect arc_rect;
  float start_angle_deg, sweep_angle_deg;
  if (getArcBetweenPointsParams(x1, y1, x2, y2, angle, radius, p1, p2, arc_rect,
                                start_angle_deg, sweep_angle_deg)) {
    arcPath.addArc(arc_rect, start_angle_deg, sweep_angle_deg);
  } else {
    arcPath.moveTo(x1, y1);
    arcPath.lineTo(x2, y2);
  }
  m_paths.push_back(arcStyledPath);

  // 2. Create arrowhead paths (for filling)
  SkPathMeasure path_measure(arcPath, false);
  if (path_measure.getLength() > 0) {
    SkPoint position_start, position_end;
    SkVector tangent_start, tangent_end;
    path_measure.getPosTan(0, &position_start, &tangent_start);
    path_measure.getPosTan(path_measure.getLength(), &position_end,
                           &tangent_end);

    if (tangent_start.length() > 1e-6) {
      StyledPath arrowheadStyledPath;
      arrowheadStyledPath.style = PathStyle::kFill;
      arrowheadStyledPath.path =
          createArrowheadPath(position_start, -tangent_start, arrowhead_size);
      m_paths.push_back(arrowheadStyledPath);
    }
    if (tangent_end.length() > 1e-6) {
      StyledPath arrowheadStyledPath;
      arrowheadStyledPath.style = PathStyle::kFill;
      arrowheadStyledPath.path =
          createArrowheadPath(position_end, tangent_end, arrowhead_size);
      m_paths.push_back(arrowheadStyledPath);
    }
  }
}

void AnnularSectorShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;

  SkRect outer_rect =
      SkRect::MakeXYWH(arc_center_x - outer_radius, arc_center_y - outer_radius,
                       2 * outer_radius, 2 * outer_radius);
  SkRect inner_rect =
      SkRect::MakeXYWH(arc_center_x - inner_radius, arc_center_y - inner_radius,
                       2 * inner_radius, 2 * inner_radius);

  styledPath.path.addArc(outer_rect, start_angle, angle);
  styledPath.path.arcTo(inner_rect, start_angle + angle, -angle, false);
  styledPath.path.close();
  m_paths.push_back(styledPath);
}

void SectorShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;

  SkRect rect = SkRect::MakeXYWH(arc_center_x - radius, arc_center_y - radius,
                                 2 * radius, 2 * radius);

  styledPath.path.addArc(rect, start_angle, angle);
  styledPath.path.lineTo(arc_center_x, arc_center_y);
  styledPath.path.close();
  m_paths.push_back(styledPath);
}

void AnnulusShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;

  styledPath.path.addCircle(center_x, center_y, outer_radius,
                            SkPathDirection::kCW);
  styledPath.path.addCircle(center_x, center_y, inner_radius,
                            SkPathDirection::kCCW);
  m_paths.push_back(styledPath);
}

void CubicBezierShape::rebuildPaths() const {
  m_paths.clear();
  StyledPath styledPath;

  styledPath.path.moveTo(x1, y1);
  styledPath.path.cubicTo(x2, y2, x3, y3, x4, y4);
  m_paths.push_back(styledPath);
}

void ArcPolygonShape::rebuildPaths() const {
  m_paths.clear();
  if (vertices.size() < 2) {
    return;
  }

  StyledPath styledPath;
  SkPath &path = styledPath.path;

  // Build the polygon by creating each segment as a separate path and then
  // stitching them together. This is more robust than using arcTo for complex
  // polygons.
  for (size_t i = 0; i < vertices.size(); ++i) {
    const auto &start_point = vertices[i];
    const auto &end_point = vertices[(i + 1) % vertices.size()];
    float angle = (i < angles.size()) ? angles[i] : 0.0f;
    float radius = (i < radii.size()) ? radii[i] : 0.0f;

    SkPath segmentPath;
    SkPoint p1, p2;
    SkRect arc_rect;
    float start_angle_deg, sweep_angle_deg;
    if (getArcBetweenPointsParams(start_point.fX, start_point.fY, end_point.fX,
                                  end_point.fY, angle, radius, p1, p2, arc_rect,
                                  start_angle_deg, sweep_angle_deg)) {
      segmentPath.addArc(arc_rect, start_angle_deg, sweep_angle_deg);
    } else {
      segmentPath.moveTo(start_point);
      segmentPath.lineTo(end_point);
    }

    // kExtend_AddPathMode will connect the last point of the main path to the
    // first point of the segment path if they don't already match.
    path.addPath(segmentPath, SkPath::kExtend_AddPathMode);
  }

  path.close();
  m_paths.push_back(styledPath);
}

QJsonObject ArcPolygonShape::serialize() const {
  QJsonObject props;
  QJsonArray jsonVertices;
  for (const auto &v : vertices) {
    QJsonObject vert_obj;
    vert_obj["x"] = v.fX;
    vert_obj["y"] = v.fY;
    jsonVertices.append(vert_obj);
  }
  props["vertices"] = jsonVertices;

  QJsonArray jsonAngles;
  for (float a : angles) {
    jsonAngles.append(a);
  }
  props["angles"] = jsonAngles;

  QJsonArray jsonRadii;
  for (float r : radii) {
    jsonRadii.append(r);
  }
  props["radii"] = jsonRadii;

  return props;
}

void ArcPolygonShape::deserialize(const QJsonObject &props) {
  vertices.clear();
  if (props.contains("vertices") && props["vertices"].isArray()) {
    QJsonArray jsonVertices = props["vertices"].toArray();
    for (const auto &val : jsonVertices) {
      QJsonObject vert_obj = val.toObject();
      vertices.push_back(
          SkPoint::Make(vert_obj["x"].toDouble(), vert_obj["y"].toDouble()));
    }
  }

  angles.clear();
  if (props.contains("angles") && props["angles"].isArray()) {
    QJsonArray jsonAngles = props["angles"].toArray();
    for (const auto &val : jsonAngles) {
      angles.push_back(val.toDouble());
    }
  }

  radii.clear();
  if (props.contains("radii") && props["radii"].isArray()) {
    QJsonArray jsonRadii = props["radii"].toArray();
    for (const auto &val : jsonRadii) {
      radii.push_back(val.toDouble());
    }
  }

  markDirty();
}

QWidget *ArcPolygonShape::createPropertyEditor(
    QWidget *parent, std::function<void(QJsonObject)> onChange) {
  auto *widget = new QWidget(parent);
  auto *layout = new QVBoxLayout(widget);

  // Vertex Table
  auto *vertex_table = new QTableWidget(parent);
  vertex_table->setColumnCount(2);
  vertex_table->setHorizontalHeaderLabels({"X", "Y"});
  vertex_table->setRowCount(vertices.size());
  for (size_t i = 0; i < vertices.size(); ++i) {
    vertex_table->setItem(
        i, 0, new QTableWidgetItem(QString::number(vertices[i].fX)));
    vertex_table->setItem(
        i, 1, new QTableWidgetItem(QString::number(vertices[i].fY)));
  }
  layout->addWidget(vertex_table);

  // Arc Properties Table
  auto *arc_table = new QTableWidget(parent);
  arc_table->setColumnCount(2);
  arc_table->setHorizontalHeaderLabels({"Angle", "Radius"});
  arc_table->setRowCount(angles.size());
  for (size_t i = 0; i < angles.size(); ++i) {
    arc_table->setItem(i, 0, new QTableWidgetItem(QString::number(angles[i])));
    arc_table->setItem(i, 1, new QTableWidgetItem(QString::number(radii[i])));
  }
  layout->addWidget(arc_table);

  // Add/Remove Buttons
  auto *add_button = new QPushButton("Add Vertex", parent);
  auto *remove_button = new QPushButton("Remove Vertex", parent);
  layout->addWidget(add_button);
  layout->addWidget(remove_button);

  auto update_shape = [this, vertex_table, arc_table, onChange]() {
    vertices.clear();
    for (int i = 0; i < vertex_table->rowCount(); ++i) {
      if (vertex_table->item(i, 0) && vertex_table->item(i, 1)) {
        vertices.push_back(
            SkPoint::Make(vertex_table->item(i, 0)->text().toFloat(),
                          vertex_table->item(i, 1)->text().toFloat()));
      }
    }
    angles.clear();
    radii.clear();
    for (int i = 0; i < arc_table->rowCount(); ++i) {
      if (arc_table->item(i, 0) && arc_table->item(i, 1)) {
        angles.push_back(arc_table->item(i, 0)->text().toFloat());
        radii.push_back(arc_table->item(i, 1)->text().toFloat());
      }
    }
    markDirty();
    if (onChange) {
      onChange(serialize());
    }
  };

  QObject::connect(add_button, &QPushButton::clicked,
                   [vertex_table, arc_table, update_shape]() {
                     int row = vertex_table->rowCount();
                     vertex_table->insertRow(row);
                     vertex_table->setItem(row, 0, new QTableWidgetItem("0"));
                     vertex_table->setItem(row, 1, new QTableWidgetItem("0"));
                     arc_table->insertRow(row);
                     arc_table->setItem(row, 0, new QTableWidgetItem("0"));
                     arc_table->setItem(row, 1, new QTableWidgetItem("0"));
                     update_shape();
                   });

  QObject::connect(remove_button, &QPushButton::clicked,
                   [vertex_table, arc_table, update_shape]() {
                     vertex_table->removeRow(vertex_table->rowCount() - 1);
                     arc_table->removeRow(arc_table->rowCount() - 1);
                     update_shape();
                   });

  QObject::connect(vertex_table, &QTableWidget::cellChanged, update_shape);
  QObject::connect(arc_table, &QTableWidget::cellChanged, update_shape);

  return widget;
}
