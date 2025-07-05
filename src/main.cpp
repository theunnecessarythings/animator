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

  w.canvas()->scene().createBackground(w.width(), w.height());

  Entity circleEntity = w.canvas()->scene().createShape("Circle", 100, 100);
  if (auto *script =
          w.canvas()->scene().reg.get<ScriptComponent>(circleEntity)) {
    script->scriptPath = "../scripts/bouncing_ball.lua";
  }

  // Refresh the scene model and select the newly created entity
  w.sceneModel()->refresh();
  QModelIndex circleIndex = w.sceneModel()->indexOfEntity(circleEntity);
  if (circleIndex.isValid()) {
    w.sceneTree()->selectionModel()->select(
        circleIndex, QItemSelectionModel::ClearAndSelect);
  }

  // Capture the initial state of the registry
  w.setInitialRegistry(w.canvas()->scene().reg);

  w.show();

  return app.exec();
}
