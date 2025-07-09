#include "canvas.h"

void SkiaCanvasWidget::setSelectedEntity(Entity entity) {
  selectedEntities_.clear();
  if (entity != kInvalidEntity)
    selectedEntities_.append(entity);

  update();
  emit canvasSelectionChanged(selectedEntities_);
}

void SkiaCanvasWidget::setSelectedEntities(const QList<Entity> &entities) {
  selectedEntities_ = entities;
  update();
  emit canvasSelectionChanged(selectedEntities_);
}

void SkiaCanvasWidget::resetSceneAndDeserialize(const QJsonObject &json) {
  scene_->clear();
  if (!json.isEmpty())
    scene_->deserialize(json);
}

void SkiaCanvasWidget::setVideoRendering(bool isRendering) {
  m_isRenderingVideo = isRendering;
}

QImage SkiaCanvasWidget::renderHighResFrame(int width, int height, float time) {
  auto imageInfo =
      SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(imageInfo);
  if (!surface) {
    qWarning() << "Failed to create offscreen SkSurface for rendering.";
    return QImage();
  }
  SkCanvas *canvas = surface->getCanvas();

  // Prepare canvas (clear, scale)
  canvas->clear(SK_ColorWHITE);

  // Calculate scale factor to fit and preserve aspect ratio
  float canvasWidth = this->width();
  float canvasHeight = this->height();
  float scale = std::min((float)width / canvasWidth, (float)height / canvasHeight);

  // Calculate offsets to center the scaled content
  float scaledWidth = canvasWidth * scale;
  float scaledHeight = canvasHeight * scale;
  float offsetX = (width - scaledWidth) / 2.0f;
  float offsetY = (height - scaledHeight) / 2.0f;

  // Apply transformations
  canvas->translate(offsetX, offsetY);
  canvas->scale(scale, scale);

  // Draw the scene
  scene_->draw(canvas, time);

  // Get pixels back
  QImage image(width, height, QImage::Format_RGBA8888);
  SkPixmap pixmap(imageInfo, image.bits(), image.bytesPerLine());

  if (!surface->readPixels(pixmap, 0, 0)) {
    qWarning() << "Failed to read pixels from offscreen SkSurface.";
    return QImage();
  }

  return image;
}

// -------------------------------------------------------------------------
//  QOpenGLWidget overrides
// -------------------------------------------------------------------------
void SkiaCanvasWidget::initializeGL() {
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
  // fontMgr = SkFontMgr::RefDefault();
}

void SkiaCanvasWidget::resizeGL(int w, int h) {
  GrGLFramebufferInfo fbInfo{static_cast<uint32_t>(defaultFramebufferObject()),
                             GL_RGBA8};
  const QSurfaceFormat fmt = context()->format();
  GrBackendRenderTarget backendRT = GrBackendRenderTargets::MakeGL(
      w, h, fmt.samples(), fmt.stencilBufferSize(), fbInfo);
  SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
  fSurface = SkSurfaces::WrapBackendRenderTarget(
      fContext.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, nullptr, &props);
  if (!fSurface)
    qWarning("Skia: failed to wrap backend FBO");
}

void SkiaCanvasWidget::paintGL() {
  if (m_isSceneBeingReset || !fSurface)
    return;

  SkCanvas *c = fSurface->getCanvas();
  c->clear(SK_ColorWHITE);

  // Render scene ------------------------------------------------------
  scene_->draw(c, currentTime_);

  // Selection overlays -----------------------------------------------
  if (!m_isRenderingVideo) {
    drawSelection(c);
    drawMarquee(c);
  }

  fContext->flushAndSubmit();
}

// -------------------------------------------------------------------------
//  Drag‑and‑drop for toolbox shapes
// -------------------------------------------------------------------------
void SkiaCanvasWidget::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasFormat("application/x-skia-shape"))
    e->acceptProposedAction();
}
void SkiaCanvasWidget::dragMoveEvent(QDragMoveEvent *e) {
  if (e->mimeData()->hasFormat("application/x-skia-shape"))
    e->acceptProposedAction();
}
void SkiaCanvasWidget::dropEvent(QDropEvent *e) {
  if (!e->mimeData()->hasFormat("application/x-skia-shape"))
    return;
  QByteArray id = e->mimeData()->data("application/x-skia-shape");
  QPointF pos = e->posF();
  e->acceptProposedAction();

  auto ent = scene_->createShape(id.toStdString(), pos.x(), pos.y());
  emit entityAdded(ent);
  emit sceneChanged();
  setSelectedEntity(ent);
}

// -------------------------------------------------------------------------
//  Mouse interaction (selection, drag, rotate, marquee)
// -------------------------------------------------------------------------
void SkiaCanvasWidget::mousePressEvent(QMouseEvent *e) {
  bool shiftPressed = e->modifiers() & Qt::ShiftModifier;
  Entity clicked = kInvalidEntity;
  isDragging_ = false;
  isRotating_ = false;
  initialTransforms_.clear();

  auto &ecs = scene_->ecs();

  // Hit detection ------------------------------------------------------
  ecs.each<TransformComponent>([&](flecs::entity ent, TransformComponent &tr) {
    if (ent.has<SceneBackgroundComponent>())
      return;
    if (ent.has<ShapeComponent>()) {
      auto &sc = ent.get<ShapeComponent>();
      SkRect originalBounds = sc.shape->getBoundingBox();
      SkMatrix m;
      m.setTranslate(tr.x, tr.y);
      m.preRotate(tr.rotation * 180 / M_PI);
      m.preScale(tr.sx, tr.sy);
      SkPoint corners[4];
      originalBounds.toQuad(corners);
      m.mapPoints(corners, corners);
      if (isPointInPolygon(SkPoint::Make(e->x(), e->y()), corners, 4))
        clicked = ent;
    }
  });

  // Rotation‑handle check (only if one selection) ---------------------
  if (selectedEntities_.size() == 1) {
    Entity sel = selectedEntities_.first();
    if (sel.is_alive() && sel.has<TransformComponent>()) {
      auto tc = sel.get<TransformComponent>();
      if (sel.has<ShapeComponent>()) {
        auto &sc = sel.get<ShapeComponent>();
        if (!sc.shape)
          return; // Skip if shape is not valid
        SkRect bb = sc.shape->getBoundingBox();
        SkMatrix m;
        m.setTranslate(tc.x, tc.y);
        m.preRotate(tc.rotation * 180 / M_PI);
        m.preScale(tc.sx, tc.sy);
        SkPoint handle = {bb.centerX(), bb.top() - 10};
        SkSpan<SkPoint> span(&handle, 1);
        m.mapPoints(span);
        SkRect hb = SkRect::MakeLTRB(handle.x() - 8, handle.y() - 8,
                                     handle.x() + 8, handle.y() + 8);
        if (hb.contains(e->x(), e->y())) {
          isRotating_ = true;
          dragStart_ = e->pos();
          initialTransforms_[sel] = {tc.x, tc.y, tc.rotation, tc.sx, tc.sy};
          emit dragStarted();
          update();
          return;
        }
      }
    }
  }

  // Selection logic ----------------------------------------------------
  if (clicked != kInvalidEntity) {
    if (shiftPressed) {
      if (selectedEntities_.contains(clicked))
        selectedEntities_.removeOne(clicked);
      else
        selectedEntities_.append(clicked);
    } else {
      if (!selectedEntities_.contains(clicked)) {
        selectedEntities_.clear();
        selectedEntities_.append(clicked);
      }
    }
    isDragging_ = true;
    dragStart_ = e->pos();
    emit dragStarted();
    for (Entity ent : selectedEntities_)
      if (ent.is_alive() && ent.has<TransformComponent>()) {
        auto tc = ent.get<TransformComponent>();
        initialTransforms_[ent] = {tc.x, tc.y, tc.rotation, tc.sx, tc.sy};
      }
  } else {
    if (!shiftPressed)
      selectedEntities_.clear();
    isMarqueeSelecting_ = true;
    marqueeStartPoint_ = e->pos();
    marqueeEndPoint_ = e->pos();
  }

  emit canvasSelectionChanged(selectedEntities_);
  update();
}

void SkiaCanvasWidget::mouseMoveEvent(QMouseEvent *e) {
  if (isRotating_ && selectedEntities_.size() == 1) {
    Entity ent = selectedEntities_.first();
    if (ent.is_alive() && ent.has<TransformComponent>()) {
      auto &tc = ent.get_mut<TransformComponent>();
      SkPoint center = {tc.x, tc.y};
      SkPoint prev = {static_cast<float>(dragStart_.x()),
                      static_cast<float>(dragStart_.y())};
      SkPoint curr = {static_cast<float>(e->pos().x()),
                      static_cast<float>(e->pos().y())};
      float angleDelta =
          std::atan2(curr.y() - center.y(), curr.x() - center.x()) -
          std::atan2(prev.y() - center.y(), prev.x() - center.x());
      if (angleDelta > M_PI)
        angleDelta -= 2 * M_PI;
      else if (angleDelta < -M_PI)
        angleDelta += 2 * M_PI;
      tc.rotation += angleDelta;
      dragStart_ = e->pos();
      emit transformChanged(ent);
      update();
    }
  } else if (isDragging_ && !selectedEntities_.isEmpty()) {
    QPointF delta = e->pos() - dragStart_;
    for (Entity ent : selectedEntities_)
      if (ent.is_alive() && ent.has<TransformComponent>()) {
        auto &tc = ent.get_mut<TransformComponent>();
        tc.x = initialTransforms_[ent].x + delta.x();
        tc.y = initialTransforms_[ent].y + delta.y();
      }
    if (selectedEntities_.size() == 1)
      emit transformChanged(selectedEntities_.first());
    update();
  } else if (isMarqueeSelecting_) {
    marqueeEndPoint_ = e->pos();
    update();
  }
}

void SkiaCanvasWidget::mouseReleaseEvent(QMouseEvent *e) {
  auto &ecs = scene_->ecs();

  if (isMarqueeSelecting_) {
    isMarqueeSelecting_ = false;
    bool shiftPressed = e->modifiers() & Qt::ShiftModifier;
    QRectF selRect(marqueeStartPoint_, marqueeEndPoint_);
    selRect = selRect.normalized();
    if (!shiftPressed)
      selectedEntities_.clear();

    ecs.each<TransformComponent>(
        [&](flecs::entity ent, TransformComponent &tr) {
          if (ent.has<SceneBackgroundComponent>())
            return;
          if (ent.has<ShapeComponent>()) {
            auto &sc = ent.get<ShapeComponent>();
            if (!sc.shape)
              return; // Skip if shape is not valid

            SkRect bb = sc.shape->getBoundingBox();
            SkMatrix m;
            m.setTranslate(tr.x, tr.y);
            m.preRotate(tr.rotation * 180 / M_PI);
            m.preScale(tr.sx, tr.sy);
            SkRect aabb;
            m.mapRect(&aabb, bb);
            if (selRect.intersects(
                    QRectF(aabb.x(), aabb.y(), aabb.width(), aabb.height())))
              if (!selectedEntities_.contains(ent))
                selectedEntities_.append(ent);
          }
        });
    emit canvasSelectionChanged(selectedEntities_);
  }

  if (isDragging_ || isRotating_) {
    if (selectedEntities_.size() == 1) {
      Entity ent = selectedEntities_.first();
      if (ent.has<TransformComponent>()) {
        auto tc = ent.get<TransformComponent>();
        const auto &init = initialTransforms_[ent];
        if (init.x != tc.x || init.y != tc.y || init.rotation != tc.rotation)
          emit transformationCompleted(ent, init.x, init.y, init.rotation, tc.x,
                                       tc.y, tc.rotation);
      }
    }
  }

  isDragging_ = isRotating_ = false;
  emit dragEnded();
  update();
}

// -------------------------------------------------------------------------
//  Overlay helpers
// -------------------------------------------------------------------------
void SkiaCanvasWidget::drawSelection(SkCanvas *c) {
  SkPaint selPaint;
  selPaint.setColor(SK_ColorRED);
  selPaint.setStroke(true);
  selPaint.setAntiAlias(true);
  selPaint.setStrokeWidth(2);
  for (Entity e : selectedEntities_)
    if (e.is_alive() && e.has<TransformComponent>()) {
      auto tc = e.get<TransformComponent>();
      SkRect bb;
      if (e.has<ShapeComponent>()) {
        auto &sc = e.get<ShapeComponent>();
        if (sc.shape)
          bb = sc.shape->getBoundingBox();
      }
      SkMatrix m;
      m.setTranslate(tc.x, tc.y);
      m.preRotate(tc.rotation * 180.f / M_PI);
      m.preScale(tc.sx, tc.sy);
      SkPoint corners[4];
      bb.toQuad(corners);
      m.mapPoints(corners, corners);
      c->drawLine(corners[0], corners[1], selPaint);
      c->drawLine(corners[1], corners[2], selPaint);
      c->drawLine(corners[2], corners[3], selPaint);
      c->drawLine(corners[3], corners[0], selPaint);
      if (selectedEntities_.size() == 1) {
        SkPaint h;
        h.setColor(SK_ColorRED);
        h.setAntiAlias(true);
        SkPoint handle = {bb.centerX(), bb.top() - 10};
        SkSpan<SkPoint> span(&handle, 1);
        m.mapPoints(span);
        c->drawCircle(handle.x(), handle.y(), 8, h);
      }
    }
}

void SkiaCanvasWidget::drawMarquee(SkCanvas *c) {
  if (!isMarqueeSelecting_)
    return;
  SkPaint fill;
  fill.setColor(SkColorSetARGB(100, 0, 0, 255));
  fill.setStyle(SkPaint::kFill_Style);
  SkPaint border;
  border.setColor(SK_ColorBLUE);
  border.setStyle(SkPaint::kStroke_Style);
  SkRect r = SkRect::MakeLTRB(marqueeStartPoint_.x(), marqueeStartPoint_.y(),
                              marqueeEndPoint_.x(), marqueeEndPoint_.y());
  c->drawRect(r, fill);
  c->drawRect(r, border);
}
