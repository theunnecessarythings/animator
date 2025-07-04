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

  void setSelectedEntity(Entity entity) { selectedEntity_ = entity; }

  void setCurrentTime(float time) { currentTime_ = time; }

protected:
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

    if (selectedEntity_ != kInvalidEntity) {
      if (auto *tc = scene_.reg.get<TransformComponent>(selectedEntity_)) {
        SkPaint p;
        p.setColor(SK_ColorRED);
        p.setStroke(true);
        p.setAntiAlias(true);
        p.setStrokeWidth(2);

        SkRect originalBounds;
        if (auto *sc = scene_.reg.get<ShapeComponent>(selectedEntity_)) {
          switch (sc->kind) {
          case ShapeComponent::Kind::Rectangle: {
            const auto &rectProps =
                std::get<RectangleProperties>(sc->properties);
            originalBounds =
                SkRect::MakeXYWH(0, 0, rectProps.width, rectProps.height);
            break;
          }
          case ShapeComponent::Kind::Circle: {
            const auto &circleProps =
                std::get<CircleProperties>(sc->properties);
            originalBounds =
                SkRect::MakeLTRB(-circleProps.radius, -circleProps.radius,
                                 circleProps.radius, circleProps.radius);
            break;
          }
          case ShapeComponent::Kind::Line: {
            const auto &lineProps = std::get<LineProperties>(sc->properties);
            originalBounds =
                SkRect::MakeLTRB(std::min(lineProps.x1, lineProps.x2),
                                 std::min(lineProps.y1, lineProps.y2),
                                 std::max(lineProps.x1, lineProps.x2),
                                 std::max(lineProps.y1, lineProps.y2));
            break;
          }
          case ShapeComponent::Kind::Bezier: {
            const auto &bezierProps =
                std::get<BezierProperties>(sc->properties);
            SkPath path;
            path.moveTo(bezierProps.x1, bezierProps.y1);
            path.cubicTo(bezierProps.cx1, bezierProps.cy1, bezierProps.cx2,
                         bezierProps.cy2, bezierProps.x2, bezierProps.y2);
            originalBounds = path.getBounds();
            break;
          }
          case ShapeComponent::Kind::Text: {
            const auto &textProps = std::get<TextProperties>(sc->properties);
            SkFont font;
            font.setSize(textProps.fontSize);
            SkRect textBounds;
            font.measureText(textProps.text.toStdString().c_str(),
                             textProps.text.toStdString().length(),
                             SkTextEncoding::kUTF8, &textBounds);
            originalBounds = textBounds;
            break;
          }
          case ShapeComponent::Kind::Image: {
            const auto &imageProps = std::get<ImageProperties>(sc->properties);
            sk_sp<SkData> data = SkData::MakeFromFileName(
                imageProps.filePath.toStdString().c_str());
            if (data) {
              // sk_sp<SkImage> image = SkImage::MakeFromEncoded(data);
              // if (image) {
              //   originalBounds =
              //       SkRect::MakeWH(image->width(), image->height());
              // }
            }
            break;
          }
          }
        }

        SkMatrix matrix;
        matrix.setTranslate(tc->x, tc->y);
        matrix.preRotate(tc->rotation * 180 / M_PI);
        matrix.preScale(tc->sx, tc->sy);

        SkPoint transformedCorners[4];
        originalBounds.toQuad(transformedCorners);
        matrix.mapPoints(transformedCorners);

        // Draw OBB
        canvas->drawLine(transformedCorners[0], transformedCorners[1], p);
        canvas->drawLine(transformedCorners[1], transformedCorners[2], p);
        canvas->drawLine(transformedCorners[2], transformedCorners[3], p);
        canvas->drawLine(transformedCorners[3], transformedCorners[0], p);

        // Rotation handle
        SkPaint handlePaint;
        handlePaint.setColor(SK_ColorRED);
        handlePaint.setAntiAlias(true);

        SkPoint rotationHandleCenter =
            SkPoint::Make(originalBounds.centerX(), originalBounds.top() - 10);
        SkSpan<SkPoint> rotationHandlePoints(&rotationHandleCenter, 1);
        matrix.mapPoints(rotationHandlePoints);

        canvas->drawCircle(rotationHandleCenter.x(), rotationHandleCenter.y(),
                           8, handlePaint);
      }
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

    ShapeComponent::Kind kind = ShapeComponent::Kind::Rectangle;
    if (id == "shape/circle")
      kind = ShapeComponent::Kind::Circle;
    else if (id == "shape/line")
      kind = ShapeComponent::Kind::Line;
    else if (id == "shape/bezier")
      kind = ShapeComponent::Kind::Bezier;
    else if (id == "shape/text")
      kind = ShapeComponent::Kind::Text;
    else if (id == "shape/image")
      kind = ShapeComponent::Kind::Image;

    scene_.createShape(kind, scenePos.x(), scenePos.y());
    emit sceneChanged();
    update();
  }

  void mousePressEvent(QMouseEvent *e) override {
    selectedEntity_ = kInvalidEntity;
    isDragging_ = false;
    isRotating_ = false;

    scene_.reg.each<TransformComponent>([&](Entity ent,
                                            TransformComponent &tr) {
      SkRect originalBounds;
      if (auto *sc = scene_.reg.get<ShapeComponent>(ent)) {
        switch (sc->kind) {
        case ShapeComponent::Kind::Rectangle: {
          const auto &rectProps = std::get<RectangleProperties>(sc->properties);
          originalBounds =
              SkRect::MakeXYWH(0, 0, rectProps.width, rectProps.height);
          break;
        }
        case ShapeComponent::Kind::Circle: {
          const auto &circleProps = std::get<CircleProperties>(sc->properties);
          originalBounds =
              SkRect::MakeLTRB(-circleProps.radius, -circleProps.radius,
                               circleProps.radius, circleProps.radius);
          break;
        }
        case ShapeComponent::Kind::Line: {
          const auto &lineProps = std::get<LineProperties>(sc->properties);
          originalBounds =
              SkRect::MakeLTRB(std::min(lineProps.x1, lineProps.x2),
                               std::min(lineProps.y1, lineProps.y2),
                               std::max(lineProps.x1, lineProps.x2),
                               std::max(lineProps.y1, lineProps.y2));
          break;
        }
        case ShapeComponent::Kind::Bezier: {
          const auto &bezierProps = std::get<BezierProperties>(sc->properties);
          SkPath path;
          path.moveTo(bezierProps.x1, bezierProps.y1);
          path.cubicTo(bezierProps.cx1, bezierProps.cy1, bezierProps.cx2,
                       bezierProps.cy2, bezierProps.x2, bezierProps.y2);
          originalBounds = path.getBounds();
          break;
        }
        case ShapeComponent::Kind::Text: {
          const auto &textProps = std::get<TextProperties>(sc->properties);
          SkFont font;
          font.setSize(textProps.fontSize);
          SkRect textBounds;
          font.measureText(textProps.text.toStdString().c_str(),
                           textProps.text.toStdString().length(),
                           SkTextEncoding::kUTF8, &textBounds);
          originalBounds = textBounds;
          break;
        }
        case ShapeComponent::Kind::Image: {
          const auto &imageProps = std::get<ImageProperties>(sc->properties);
          sk_sp<SkData> data = SkData::MakeFromFileName(
              imageProps.filePath.toStdString().c_str());
          if (data) {
            // sk_sp<SkImage> image = SkImage::MakeFromEncoded(data);
            // if (image) {
            //   originalBounds = SkRect::MakeWH(image->width(),
            //   image->height());
            // }
          }
          break;
        }
        }
      }

      SkMatrix matrix;
      matrix.setTranslate(tr.x, tr.y);
      matrix.preRotate(tr.rotation * 180 / M_PI);
      matrix.preScale(tr.sx, tr.sy);

      SkPoint transformedCorners[4];
      originalBounds.toQuad(transformedCorners);
      matrix.mapPoints(transformedCorners);

      SkRect bounds;
      bounds.setBounds(transformedCorners);

      SkPoint rotationHandleCenter =
          SkPoint::Make(originalBounds.centerX(), originalBounds.top() - 10);
      SkSpan<SkPoint> rotationHandlePoints(&rotationHandleCenter, 1);
      matrix.mapPoints(rotationHandlePoints);

      SkRect handleBounds = SkRect::MakeLTRB(
          rotationHandleCenter.x() - 8, rotationHandleCenter.y() - 8,
          rotationHandleCenter.x() + 8, rotationHandleCenter.y() + 8);
      if (handleBounds.contains(e->x(), e->y())) {
        selectedEntity_ = ent;
        isRotating_ = true;
        dragStart_ = e->pos();
        return;
      }

      if (bounds.contains(e->x(), e->y())) {
        selectedEntity_ = ent;
        isDragging_ = true;
        dragStart_ = e->pos();
      }
    });
    emit canvasSelectionChanged(selectedEntity_);
    update();
  }

  void mouseMoveEvent(QMouseEvent *e) override {
    if (isRotating_ && selectedEntity_ != kInvalidEntity) {
      if (auto *c = scene_.reg.get<TransformComponent>(selectedEntity_)) {
        SkPoint center = SkPoint::Make(c->x, c->y);
        SkPoint prevPoint = SkPoint::Make(dragStart_.x(), dragStart_.y());
        SkPoint currPoint = SkPoint::Make(e->pos().x(), e->pos().y());

        SkVector vecPrev = prevPoint - center;
        SkVector vecCurr = currPoint - center;

        float anglePrev = std::atan2(vecPrev.y(), vecPrev.x());
        float angleCurr = std::atan2(vecCurr.y(), vecCurr.x());

        float angleDelta = angleCurr - anglePrev;

        // Normalize angleDelta to be within -PI to PI
        if (angleDelta > M_PI) {
          angleDelta -= 2 * M_PI;
        } else if (angleDelta < -M_PI) {
          angleDelta += 2 * M_PI;
        }

        c->rotation += angleDelta;

        dragStart_ = e->pos();
        emit transformChanged(selectedEntity_);
        update();
      }
    } else if (isDragging_ && selectedEntity_ != kInvalidEntity) {
      if (auto *c = scene_.reg.get<TransformComponent>(selectedEntity_)) {
        QPointF delta = e->pos() - dragStart_;
        c->x += delta.x();
        c->y += delta.y();
        dragStart_ = e->pos();
        emit transformChanged(selectedEntity_);
        update();
      }
    }
  }

  void mouseReleaseEvent(QMouseEvent *e) override {
    isDragging_ = false;
    isRotating_ = false;
    update();
  }

signals:
  void sceneChanged();
  void transformChanged(Entity entity);
  void canvasSelectionChanged(Entity entity);

private:
  sk_sp<GrDirectContext> fContext;
  sk_sp<SkSurface> fSurface;
  sk_sp<const GrGLInterface> iface;
  sk_sp<SkFontMgr> fontMgr;
  Scene scene_;
  Entity selectedEntity_ = kInvalidEntity;
  bool isDragging_ = false;
  bool isRotating_ = false;
  QPointF dragStart_;
  float currentTime_ = 0.0f;
};
