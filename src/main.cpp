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
  w.canvas()->scene().createBackground(10000, 10000);
  w.captureInitialScene();
  w.sceneModel()->refresh();

  w.show();
  return app.exec();
}
