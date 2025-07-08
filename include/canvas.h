#pragma once
#include "scene.h"
#include "shapes.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkPaint.h"

#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QMap>
#include <QMimeData>
#include <QMouseEvent>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QSurfaceFormat>

#include <cmath>
#include <memory>

using namespace skgpu::ganesh;

class SkiaCanvasWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT
public:
  explicit SkiaCanvasWidget(QWidget *parent = nullptr)
      : QOpenGLWidget(parent), scene_(std::make_unique<Scene>()) {
    setAcceptDrops(true);
  }

  Scene &scene() { return *scene_; }
  const QList<Entity> &getSelectedEntities() const { return selectedEntities_; }
  void setSceneResetting(bool resetting) { m_isSceneBeingReset = resetting; }
  void setCurrentTime(float t) { currentTime_ = t; }

  void setSelectedEntity(Entity entity);
  void setSelectedEntities(const QList<Entity> &entities);
  void resetSceneAndDeserialize(const QJsonObject &json);

protected:
  // -------------------------------------------------------------------------
  //  Geometry helpers
  // -------------------------------------------------------------------------
  static inline bool isPointInPolygon(const SkPoint &pt, const SkPoint poly[],
                                      int n) {
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      if (((poly[i].y() > pt.y()) != (poly[j].y() > pt.y())) &&
          (pt.x() < (poly[j].x() - poly[i].x()) * (pt.y() - poly[i].y()) /
                            (poly[j].y() - poly[i].y()) +
                        poly[i].x()))
        inside = !inside;
    }
    return inside;
  }

  // -------------------------------------------------------------------------
  //  QOpenGLWidget overrides
  // -------------------------------------------------------------------------
  void initializeGL() override;

  void resizeGL(int w, int h) override;

  void paintGL() override;

  // -------------------------------------------------------------------------
  //  Drag‑and‑drop for toolbox shapes
  // -------------------------------------------------------------------------
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;

  // -------------------------------------------------------------------------
  //  Mouse interaction (selection, drag, rotate, marquee)
  // -------------------------------------------------------------------------
  void mousePressEvent(QMouseEvent *e) override;

  void mouseMoveEvent(QMouseEvent *e) override;

  void mouseReleaseEvent(QMouseEvent *e) override;

  // -------------------------------------------------------------------------
  //  Overlay helpers
  // -------------------------------------------------------------------------
  void drawSelection(SkCanvas *c);

  void drawMarquee(SkCanvas *c);

signals:
  void sceneChanged();
  void transformChanged(Entity entity);
  void canvasSelectionChanged(const QList<Entity> &entities);
  void transformationCompleted(Entity entity, float oldX, float oldY,
                               float oldRot, float newX, float newY,
                               float newRot);
  void dragStarted();
  void dragEnded();

private:
  struct TransformData {
    float x, y, rotation, sx, sy;
  };

  // Skia / GPU
  sk_sp<GrDirectContext> fContext;
  sk_sp<SkSurface> fSurface;
  sk_sp<const GrGLInterface> iface;
  sk_sp<SkFontMgr> fontMgr;

  // Scene wrapper
  std::unique_ptr<Scene> scene_;

  // Interaction state
  QList<Entity> selectedEntities_;
  bool isDragging_ = false, isRotating_ = false, isMarqueeSelecting_ = false;
  bool m_isSceneBeingReset = false;
  QPointF dragStart_, marqueeStartPoint_, marqueeEndPoint_;
  float currentTime_ = 0.f;
  QMap<Entity, TransformData> initialTransforms_;
};
