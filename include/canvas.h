#pragma once

#include "ecs.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkPaint.h"

#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"

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

using namespace skgpu::ganesh;

class SkiaCanvasWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT
public:
  explicit SkiaCanvasWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
    setAcceptDrops(true);
  }

  Scene &scene() { return scene_; }

  void setSelectedEntity(Entity entity) {
    selectedEntities_.clear();
    if (entity != kInvalidEntity)
      selectedEntities_.append(entity);

    update();
    emit canvasSelectionChanged(selectedEntities_);
  }

  void setSelectedEntities(const QList<Entity> &entities) {
    selectedEntities_ = entities;
    update();
    emit canvasSelectionChanged(selectedEntities_);
  }

  const QList<Entity> &getSelectedEntities() const { return selectedEntities_; }

  void setCurrentTime(float t) { currentTime_ = t; }

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
  void initializeGL() override {
    initializeOpenGLFunctions();

    QOpenGLContext *qtCtx = QOpenGLContext::currentContext();
    Q_ASSERT(qtCtx && qtCtx->isValid());

    auto loader = [](void *ctx, const char name[]) -> GrGLFuncPtr {
      return reinterpret_cast<GrGLFuncPtr>(
          static_cast<QOpenGLContext *>(ctx)->getProcAddress(QByteArray(name)));
    };

    iface = GrGLMakeAssembledInterface(qtCtx, loader);
    if (!iface)
      qFatal("Skia: couldn’t assemble GL interface");

    qDebug() << "GL vendor:   "
             << reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    qDebug() << "GL renderer: "
             << reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    qDebug() << "GL version:  "
             << reinterpret_cast<const char *>(glGetString(GL_VERSION));

    fContext = GrDirectContexts::MakeGL(iface);
    if (!fContext)
      qFatal("Skia: couldn’t create GrDirectContext");

    SkGraphics::Init();
    fontMgr = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
  }

  void resizeGL(int w, int h) override {
    GrGLFramebufferInfo fbInfo;
    fbInfo.fFBOID = defaultFramebufferObject();
    fbInfo.fFormat = GL_RGBA8;

    const QSurfaceFormat fmt = context()->format();
    const int samples = fmt.samples();
    const int stencilBits = fmt.stencilBufferSize();

    GrBackendRenderTarget backendRT =
        GrBackendRenderTargets::MakeGL(w, h, samples, stencilBits, fbInfo);

    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);

    fSurface = SkSurfaces::WrapBackendRenderTarget(
        fContext.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, nullptr, &props);

    if (!fSurface)
      qWarning("Skia: failed to wrap backend FBO");
  }

  void paintGL() override {
    if (!fSurface)
      return;

    SkCanvas *c = fSurface->getCanvas();
    c->clear(SK_ColorWHITE);

    // render scene --------------------------------------------------------
    scene_.renderer.render(c, currentTime_);

    // selection outlines --------------------------------------------------
    SkPaint selPaint;
    selPaint.setColor(SK_ColorRED);
    selPaint.setStroke(true);
    selPaint.setAntiAlias(true);
    selPaint.setStrokeWidth(2);

    for (Entity e : selectedEntities_) {
      if (auto *tc = scene_.reg.get<TransformComponent>(e)) {
        SkRect bb;
        if (auto *sc = scene_.reg.get<ShapeComponent>(e))
          if (sc->shape)
            bb = sc->shape->getBoundingBox();

        SkMatrix m;
        m.setTranslate(tc->x, tc->y);
        m.preRotate(tc->rotation * 180.f / M_PI);
        m.preScale(tc->sx, tc->sy);

        SkPoint corners[4];
        bb.toQuad(corners);
        m.mapPoints(corners, corners);

        c->drawLine(corners[0], corners[1], selPaint);
        c->drawLine(corners[1], corners[2], selPaint);
        c->drawLine(corners[2], corners[3], selPaint);
        c->drawLine(corners[3], corners[0], selPaint);

        // rotation handle if single selection
        if (selectedEntities_.size() == 1) {
          SkPaint h;
          h.setColor(SK_ColorRED);
          h.setAntiAlias(true);

          SkPoint handle = {bb.centerX(), bb.top() - 10};
          SkSpan<SkPoint> handleSpan(&handle, 1);
          m.mapPoints(handleSpan);
          c->drawCircle(handle.x(), handle.y(), 8, h);
        }
      }
    }

    // marquee -------------------------------------------------------------
    if (isMarqueeSelecting_) {
      SkPaint fill;
      fill.setColor(SkColorSetARGB(100, 0, 0, 255));
      fill.setStyle(SkPaint::kFill_Style);
      SkPaint border;
      border.setColor(SK_ColorBLUE);
      border.setStyle(SkPaint::kStroke_Style);

      SkRect r =
          SkRect::MakeLTRB(marqueeStartPoint_.x(), marqueeStartPoint_.y(),
                           marqueeEndPoint_.x(), marqueeEndPoint_.y());
      c->drawRect(r, fill);
      c->drawRect(r, border);
    }

    fContext->flushAndSubmit();
  }

  // -------------------------------------------------------------------------
  //  Drag-and-drop for toolbox shapes
  // -------------------------------------------------------------------------
  void dragEnterEvent(QDragEnterEvent *e) override {
    if (e->mimeData()->hasFormat("application/x-skia-shape"))
      e->acceptProposedAction();
  }
  void dragMoveEvent(QDragMoveEvent *e) override {
    if (e->mimeData()->hasFormat("application/x-skia-shape"))
      e->acceptProposedAction();
  }
  void dropEvent(QDropEvent *e) override {
    if (!e->mimeData()->hasFormat("application/x-skia-shape"))
      return;

    QByteArray id = e->mimeData()->data("application/x-skia-shape");
    QPointF pos = e->posF();
    e->acceptProposedAction();

    scene_.createShape(id.toStdString(), pos.x(), pos.y());
    emit sceneChanged();
    update();
  }

  // -------------------------------------------------------------------------
  //  Mouse interaction (selection, drag, rotate, marquee)
  // -------------------------------------------------------------------------
  void mousePressEvent(QMouseEvent *e) override {
    bool shiftPressed = e->modifiers() & Qt::ShiftModifier;
    Entity clickedEntity = kInvalidEntity;
    isDragging_ = false;
    isRotating_ = false;
    initialTransforms_.clear();

    // --- Hit detection loop ---
    scene_.reg.each<TransformComponent>(
        [&](Entity ent, TransformComponent &tr) {
          if (scene_.reg.has<SceneBackgroundComponent>(ent)) {
            return;
          }

          if (auto *sc = scene_.reg.get<ShapeComponent>(ent)) {
            if (!sc->shape)
              return; // Skip if shape is not valid

            SkRect originalBounds = sc->shape->getBoundingBox();

            SkMatrix matrix;
            matrix.setTranslate(tr.x, tr.y);
            matrix.preRotate(tr.rotation * 180 / M_PI);
            matrix.preScale(tr.sx, tr.sy);

            SkPoint transformedCorners[4];
            originalBounds.toQuad(transformedCorners);
            matrix.mapPoints(transformedCorners, transformedCorners);

            if (isPointInPolygon(SkPoint::Make(e->x(), e->y()),
                                 transformedCorners, 4)) {
              clickedEntity = ent;
            }
          }
        });

    // --- Rotation handle check ---
    if (selectedEntities_.count() == 1) {
      Entity selectedEntity = selectedEntities_.first();
      if (auto *tc = scene_.reg.get<TransformComponent>(selectedEntity)) {
        if (auto *sc = scene_.reg.get<ShapeComponent>(selectedEntity)) {
          if (sc->shape) {
            // POLYMORPHIC CALL: Get bounds for rotation handle calculation
            SkRect originalBounds = sc->shape->getBoundingBox();

            SkMatrix matrix;
            matrix.setTranslate(tc->x, tc->y);
            matrix.preRotate(tc->rotation * 180 / M_PI);
            matrix.preScale(tc->sx, tc->sy);

            SkPoint rotationHandleCenter = SkPoint::Make(
                originalBounds.centerX(), originalBounds.top() - 10);
            SkSpan<SkPoint> rotationHandleCenterSpan(&rotationHandleCenter, 1);
            matrix.mapPoints(rotationHandleCenterSpan);

            SkRect handleBounds = SkRect::MakeLTRB(
                rotationHandleCenter.x() - 8, rotationHandleCenter.y() - 8,
                rotationHandleCenter.x() + 8, rotationHandleCenter.y() + 8);

            if (handleBounds.contains(e->x(), e->y())) {
              isRotating_ = true;
              emit dragStarted();
              dragStart_ = e->pos();
              initialTransforms_[selectedEntity] = {tc->x, tc->y, tc->rotation,
                                                    tc->sx, tc->sy};
              update();
              return; // Don't process other selection logic
            }
          }
        }
      }
    }

    // --- Selection logic ---
    if (clickedEntity != kInvalidEntity) { // An entity was clicked
      if (shiftPressed) {
        if (selectedEntities_.contains(clickedEntity)) {
          selectedEntities_.removeOne(clickedEntity);
        } else {
          selectedEntities_.append(clickedEntity);
        }
      } else {
        if (!selectedEntities_.contains(clickedEntity)) {
          selectedEntities_.clear();
          selectedEntities_.append(clickedEntity);
        }
      }
      isDragging_ = true;
      emit dragStarted();
      dragStart_ = e->pos();
      for (Entity entity : selectedEntities_) {
        if (auto *tc = scene_.reg.get<TransformComponent>(entity)) {
          initialTransforms_[entity] = {tc->x, tc->y, tc->rotation, tc->sx,
                                        tc->sy};
        }
      }
    } else { // Background was clicked
      if (!shiftPressed) {
        selectedEntities_.clear();
      }
      isMarqueeSelecting_ = true;
      marqueeStartPoint_ = e->pos();
      marqueeEndPoint_ = e->pos();
    }

    emit canvasSelectionChanged(selectedEntities_);
    update();
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if (isRotating_ && selectedEntities_.count() == 1) {
      Entity entity = selectedEntities_.first();
      if (auto *c = scene_.reg.get<TransformComponent>(entity)) {
        SkPoint center = SkPoint::Make(c->x, c->y);
        SkPoint prevPoint = SkPoint::Make(dragStart_.x(), dragStart_.y());
        SkPoint currPoint = SkPoint::Make(e->pos().x(), e->pos().y());

        SkVector vecPrev = prevPoint - center;
        SkVector vecCurr = currPoint - center;

        float anglePrev = std::atan2(vecPrev.y(), vecPrev.x());
        float angleCurr = std::atan2(vecCurr.y(), vecCurr.x());

        float angleDelta = angleCurr - anglePrev;

        if (angleDelta > M_PI) {
          angleDelta -= 2 * M_PI;
        } else if (angleDelta < -M_PI) {
          angleDelta += 2 * M_PI;
        }

        c->rotation += angleDelta;

        dragStart_ = e->pos();
        emit transformChanged(entity);
        update();
      }
    } else if (isDragging_ && !selectedEntities_.isEmpty()) {
      QPointF delta = e->pos() - dragStart_;
      for (Entity entity : selectedEntities_) {
        if (auto *c = scene_.reg.get<TransformComponent>(entity)) {
          c->x = initialTransforms_[entity].x + delta.x();
          c->y = initialTransforms_[entity].y + delta.y();
        }
      }
      if (selectedEntities_.count() == 1) {
        emit transformChanged(selectedEntities_.first());
      }
      update();
    } else if (isMarqueeSelecting_) {
      marqueeEndPoint_ = e->pos();
      update();
    }
  }

  void mouseReleaseEvent(QMouseEvent *e) override {
    if (isMarqueeSelecting_) {
      isMarqueeSelecting_ = false;
      bool shiftPressed = e->modifiers() & Qt::ShiftModifier;

      QRectF selectionRect =
          QRectF(marqueeStartPoint_, marqueeEndPoint_).normalized();

      if (!shiftPressed) {
        selectedEntities_.clear();
      }

      scene_.reg.each<TransformComponent>([&](Entity ent,
                                              TransformComponent &tr) {
        if (scene_.reg.has<SceneBackgroundComponent>(ent)) {
          return;
        }

        if (auto *sc = scene_.reg.get<ShapeComponent>(ent)) {
          if (!sc->shape)
            return; // Skip if shape is not valid

          SkRect originalBounds = sc->shape->getBoundingBox();
          SkMatrix matrix;
          matrix.setTranslate(tr.x, tr.y);
          matrix.preRotate(tr.rotation * 180 / M_PI);
          matrix.preScale(tr.sx, tr.sy);

          SkRect entityAABB;
          matrix.mapRect(&entityAABB, originalBounds);

          if (selectionRect.intersects(QRectF(entityAABB.x(), entityAABB.y(),
                                              entityAABB.width(),
                                              entityAABB.height()))) {
            if (!selectedEntities_.contains(ent)) {
              selectedEntities_.append(ent);
            }
          }
        }
      });
      emit canvasSelectionChanged(selectedEntities_);
    }

    if (isDragging_ || isRotating_) {
      if (selectedEntities_.count() == 1) {
        Entity entity = selectedEntities_.first();
        if (auto *tc = scene_.reg.get<TransformComponent>(entity)) {
          const auto &initial = initialTransforms_[entity];
          if (initial.x != tc->x || initial.y != tc->y ||
              initial.rotation != tc->rotation) {
            emit transformationCompleted(entity, initial.x, initial.y,
                                         initial.rotation, tc->x, tc->y,
                                         tc->rotation);
          }
        }
      }
    }

    isDragging_ = false;
    isRotating_ = false;
    emit dragEnded();
    update();
  }

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
  // Reusable struct for original transforms during drag/rotate
  struct TransformData {
    float x, y, rotation, sx, sy;
  };

  // Skia / GPU ----------------------------------------------------------------
  sk_sp<GrDirectContext> fContext;
  sk_sp<SkSurface> fSurface;
  sk_sp<const GrGLInterface> iface;
  sk_sp<SkFontMgr> fontMgr;

  // Scene ---------------------------------------------------------------------
  Scene scene_;

  // Interaction state ---------------------------------------------------------
  QList<Entity> selectedEntities_;
  bool isDragging_ = false;
  bool isRotating_ = false;
  bool isMarqueeSelecting_ = false;
  QPointF dragStart_;
  QPointF marqueeStartPoint_;
  QPointF marqueeEndPoint_;
  float currentTime_ = 0.f;
  QMap<Entity, TransformData> initialTransforms_;
};
