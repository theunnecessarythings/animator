#include "window.h"
#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char **argv) {
  QSurfaceFormat fmt;
  fmt.setRenderableType(QSurfaceFormat::OpenGL);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setVersion(3, 3);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);

  MainWindow w;
  w.resize(1024, 768);

  // Build default scene -----------------------------------------------------
  w.canvas()->scene().createBackground(w.width(), w.height());
  Entity circleEntity = w.canvas()->scene().createShape("Circle", 100, 100);
  if (circleEntity.has<ScriptComponent>()) {
    auto &scr = circleEntity.get_mut<ScriptComponent>();
    scr.scriptPath = "../scripts/bouncing_ball.lua";
  }

  w.sceneModel()->refresh();

  // Select the newly-created circle in the scene tree
  if (QModelIndex circleIdx = w.sceneModel()->indexOfEntity(circleEntity);
      circleIdx.isValid())
    w.sceneTree()->selectionModel()->select(
        circleIdx, QItemSelectionModel::ClearAndSelect);

  w.show();
  return app.exec();
}
