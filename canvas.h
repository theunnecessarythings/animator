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
using namespace skgpu::ganesh;

#include <QDragEnterEvent>
#include <QMap>
#include <QMimeData>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QSurfaceFormat>

#include <QMouseEvent>

#include <cmath>

class SkiaCanvasWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT
public:
  explicit SkiaCanvasWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
    setAcceptDrops(true);
  }

  Scene &scene() { return scene_; }

  void setSelectedEntity(Entity entity) {
    selectedEntities_.clear();
    if (entity != kInvalidEntity) {
      selectedEntities_.append(entity);
    }
    update();
    emit canvasSelectionChanged(selectedEntities_);
  }

  void setSelectedEntities(const QList<Entity> &entities) {
    selectedEntities_ = entities;
    update();
    emit canvasSelectionChanged(selectedEntities_);
  }

  const QList<Entity> &getSelectedEntities() const { return selectedEntities_; }

  void setCurrentTime(float time) { currentTime_ = time; }

protected:
  inline bool isPointInPolygon(const SkPoint &point, const SkPoint polygon[],
                               int n) {
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      if (((polygon[i].y() > point.y()) != (polygon[j].y() > point.y())) &&
          (point.x() < (polygon[j].x() - polygon[i].x()) *
                               (point.y() - polygon[i].y()) /
                               (polygon[j].y() - polygon[i].y()) +
                           polygon[i].x())) {
        inside = !inside;
      }
    }
    return inside;
  }

  void initializeGL() override {
    initializeOpenGLFunctions();
    QOpenGLContext *qtCtx = QOpenGLContext::currentContext();
    Q_ASSERT(qtCtx && qtCtx->isValid());

    auto loader = [](void *ctx, const char name[]) -> GrGLFuncPtr {
      return reinterpret_cast<GrGLFuncPtr>(
          static_cast<QOpenGLContext *>(ctx)->getProcAddress(QByteArray(name)));
    };

    iface = GrGLMakeAssembledInterface(qtCtx, loader);
    if (!iface) {
      qFatal("Skia: couldn’t assemble GL interface");
    }

    qDebug() << "GL vendor:"
             << reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    qDebug() << "GL renderer:"
             << reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    qDebug() << "GL version:"
             << reinterpret_cast<const char *>(glGetString(GL_VERSION));
    if (!iface) {
      qWarning("Failed to create GrGLInterface");
      return;
    }
    fContext = GrDirectContexts::MakeGL(iface);
    if (!fContext) {
      qWarning("Failed to create GrDirectContext");
      return;
    }

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
        GrBackendRenderTargets::MakeGL(w, h,
                                       /*samples*/ samples,
                                       /*stencils*/ stencilBits, fbInfo);
    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);

    fSurface = SkSurfaces::WrapBackendRenderTarget(
        fContext.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, nullptr, &props);

    if (!fSurface)
      qWarning("Failed to wrap backend FBO – "
               "double-check format / MSAA / stencil");
  }

  void paintGL() override {
    if (!fSurface)
      return;
    auto *canvas = fSurface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    scene_.renderer.render(canvas, currentTime_);

    // Draw selection highlights for all selected entities
    for (Entity entity : selectedEntities_) {
      if (auto *tc = scene_.reg.get<TransformComponent>(entity)) {
        SkPaint p;
        p.setColor(SK_ColorRED);
        p.setStroke(true);
        p.setAntiAlias(true);
        p.setStrokeWidth(2);

        SkRect originalBounds;
        if (auto *sc = scene_.reg.get<ShapeComponent>(entity)) {
          if (sc->shape) {
            originalBounds = sc->shape->getBoundingBox();
          }
        }

        SkMatrix matrix;
        matrix.setTranslate(tc->x, tc->y);
        matrix.preRotate(tc->rotation * 180 / M_PI);
        matrix.preScale(tc->sx, tc->sy);

        SkPoint transformedCorners[4];
        originalBounds.toQuad(transformedCorners);
        matrix.mapPoints(transformedCorners, transformedCorners);

        // Draw OBB
        canvas->drawLine(transformedCorners[0], transformedCorners[1], p);
        canvas->drawLine(transformedCorners[1], transformedCorners[2], p);
        canvas->drawLine(transformedCorners[2], transformedCorners[3], p);
        canvas->drawLine(transformedCorners[3], transformedCorners[0], p);

        // Rotation handle (only if one entity is selected)
        if (selectedEntities_.count() == 1) {
          SkPaint handlePaint;
          handlePaint.setColor(SK_ColorRED);
          handlePaint.setAntiAlias(true);

          SkPoint rotationHandleCenter = SkPoint::Make(
              originalBounds.centerX(), originalBounds.top() - 10);
          SkSpan<SkPoint> rotationHandleCenterSpan(&rotationHandleCenter, 1);
          matrix.mapPoints(rotationHandleCenterSpan);

          canvas->drawCircle(rotationHandleCenter.x(), rotationHandleCenter.y(),
                             8, handlePaint);
        }
      }
    }

    // Draw marquee selection rectangle
    if (isMarqueeSelecting_) {
      SkPaint p;
      p.setColor(SkColorSetARGB(100, 0, 0, 255)); // semi-transparent blue
      p.setStyle(SkPaint::kFill_Style);
      canvas->drawRect(
          SkRect::MakeLTRB(marqueeStartPoint_.x(), marqueeStartPoint_.y(),
                           marqueeEndPoint_.x(), marqueeEndPoint_.y()),
          p);

      p.setColor(SK_ColorBLUE);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(1);
      canvas->drawRect(
          SkRect::MakeLTRB(marqueeStartPoint_.x(), marqueeStartPoint_.y(),
                           marqueeEndPoint_.x(), marqueeEndPoint_.y()),
          p);
    }

    fContext->flushAndSubmit();
  }

  void dragEnterEvent(QDragEnterEvent *e) override {
    if (e->mimeData()->hasFormat("application/x-skia-shape"))
      e->acceptProposedAction(); // 🔑 must accept here
  }

  void dragMoveEvent(QDragMoveEvent *e) override {
    if (e->mimeData()->hasFormat("application/x-skia-shape"))
      e->acceptProposedAction(); // some platforms need this too
  }

  void dropEvent(QDropEvent *e) override {
    if (!e->mimeData()->hasFormat("application/x-skia-shape"))
      return;

    const QByteArray id = e->mimeData()->data("application/x-skia-shape");
    const QPointF scenePos = e->posF();
    e->acceptProposedAction();
    scene_.createShape(id.toStdString(), scenePos.x(), scenePos.y());

    emit sceneChanged();
    update();
  }

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
    update();
  }
signals:
  void sceneChanged();
  void transformChanged(Entity entity);
  void canvasSelectionChanged(const QList<Entity> &entities);
  void transformationCompleted(Entity entity, float oldX, float oldY,
                               float oldRotation, float newX, float newY,
                               float newRotation);

private:
  struct TransformData {
    float x, y, rotation, sx, sy;
  };

  sk_sp<GrDirectContext> fContext;
  sk_sp<SkSurface> fSurface;
  sk_sp<const GrGLInterface> iface;
  sk_sp<SkFontMgr> fontMgr;
  Scene scene_;

  QList<Entity> selectedEntities_;
  bool isDragging_ = false;
  bool isRotating_ = false;
  bool isMarqueeSelecting_ = false;
  QPointF dragStart_;
  QPointF marqueeStartPoint_;
  QPointF marqueeEndPoint_;
  float currentTime_ = 0.0f;
  QMap<Entity, TransformData> initialTransforms_;
};
