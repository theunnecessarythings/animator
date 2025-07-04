#pragma once

#include "canvas.h"
#include "scene_model.h"
#include "toolbox.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>

#include <QSlider>
#include <QTimer>

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
    m_canvas = new SkiaCanvasWidget(this);
    setCentralWidget(m_canvas);

    createMenus();
    createToolbox();
    createSceneDock();
    createPropertiesDock();
    createTimelineDock();

    setWindowTitle(tr("Skia Animation Studio"));
  }

  SkiaCanvasWidget *canvas() const { return m_canvas; }
  SceneModel *sceneModel() const { return m_sceneModel; }
  QTreeView *sceneTree() const { return m_sceneTree; }

  void setInitialRegistry(const Registry &reg) { m_initialRegistry = reg; }

private:
  void createMenus() {
    // --- File -----------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"));
    fileMenu->addAction(tr("&Open…"));
    fileMenu->addAction(tr("&Save"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), qApp, &QCoreApplication::quit);

    // --- Edit -----------------------------------------------------------
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("Undo"));
    editMenu->addAction(tr("Redo"));
    editMenu->addSeparator();
    editMenu->addAction(tr("Cut"));
    editMenu->addAction(tr("Copy"));
    editMenu->addAction(tr("Paste"));

    // --- View -----------------------------------------------------------
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Zoom In"));
    viewMenu->addAction(tr("Zoom Out"));
    viewMenu->addAction(tr("Reset View"));

    // --- Playback -------------------------------------------------------
    QMenu *playMenu = menuBar()->addMenu(tr("&Playback"));
    playMenu->addAction(tr("Play"));
    playMenu->addAction(tr("Pause"));
    playMenu->addAction(tr("Stop"));

    // --- Help -----------------------------------------------------------
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("About"));
  }

  void createToolbox() {
    auto *toolDock = new QDockWidget(tr("Toolbox"), this);
    toolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *tools = new ToolboxWidget(toolDock);
    tools->setIconSize({32, 32});
    tools->setGridSize({72, 72});
    tools->setSpacing(4);
    toolDock->setWidget(tools);
    toolDock->setMaximumWidth(72);
    addDockWidget(Qt::LeftDockWidgetArea, toolDock);
  }

  void createSceneDock() {
    auto *sceneDock = new QDockWidget(tr("Scene"), this);
    sceneDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                               Qt::RightDockWidgetArea);

    auto *tree = new QTreeView(sceneDock);

    m_sceneModel = new SceneModel(&m_canvas->scene(), tree);
    tree->setModel(m_sceneModel);
    tree->setHeaderHidden(true);

    m_sceneTree = tree;
    sceneDock->setWidget(tree);
    sceneDock->setMaximumHeight(200); // Decrease height of outline
    addDockWidget(Qt::RightDockWidgetArea, sceneDock);

    connect(m_canvas, &SkiaCanvasWidget::sceneChanged, m_sceneModel,
            &SceneModel::refresh);

    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSceneSelectionChanged);

    connect(m_canvas, &SkiaCanvasWidget::transformChanged, this,
            &MainWindow::onTransformChanged);
    connect(m_canvas, &SkiaCanvasWidget::canvasSelectionChanged, this,
            &MainWindow::onCanvasSelectionChanged);
  }

  void createPropertiesDock() {
    auto *propDock = new QDockWidget(tr("Properties"), this);
    propDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *propWidget = new QWidget(propDock);
    m_propsLayout = new QFormLayout(propWidget);
    propWidget->setLayout(m_propsLayout);

    propDock->setWidget(propWidget);
    propDock->setMinimumHeight(400); // Increase height of properties window
    addDockWidget(Qt::RightDockWidgetArea, propDock);
  }

  void createTimelineDock() {
    auto *timelineDock = new QDockWidget(tr("Timeline"), this);
    timelineDock->setAllowedAreas(Qt::BottomDockWidgetArea);

    auto *timelineWidget = new QWidget(timelineDock);
    auto *timelineLayout = new QHBoxLayout(timelineWidget);

    m_playPauseButton = new QPushButton("Play");
    connect(m_playPauseButton, &QPushButton::clicked, this,
            &MainWindow::onPlayPauseButtonClicked);
    timelineLayout->addWidget(m_playPauseButton);

    m_stopResetButton = new QPushButton("Stop");
    connect(m_stopResetButton, &QPushButton::clicked, this,
            &MainWindow::onStopResetButtonClicked);
    timelineLayout->addWidget(m_stopResetButton);

    m_timeDisplayLabel = new QLabel("0.00s / 0.00s");
    timelineLayout->addWidget(m_timeDisplayLabel);

    m_timelineSlider = new QSlider(Qt::Horizontal);
    m_timelineSlider->setRange(
        0, static_cast<int>(m_animationDuration * 100)); // 100 units per second
    m_timelineSlider->setValue(0);
    connect(m_timelineSlider, &QSlider::valueChanged, this,
            &MainWindow::onTimelineSliderMoved);
    timelineLayout->addWidget(m_timelineSlider);

    timelineDock->setWidget(timelineWidget);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);

    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this,
            &MainWindow::onAnimationTimerTimeout);
    m_animationTimer->setInterval(16); // ~60 FPS
  }

private slots:
  void onPlayPauseButtonClicked() {
    if (m_isPlaying) {
      m_animationTimer->stop();
      m_playPauseButton->setText("Play");
    } else {
      m_animationTimer->start();
      m_playPauseButton->setText("Pause");
    }
    m_isPlaying = !m_isPlaying;
  }

  void onStopResetButtonClicked() {
    m_animationTimer->stop();
    m_isPlaying = false;
    m_playPauseButton->setText("Play");
    m_currentTime = 0.0f;
    m_canvas->setCurrentTime(m_currentTime);
    resetScene(); // Reset the scene to its initial state
    m_canvas->update();
    m_timelineSlider->setValue(0);
    updateTimeDisplay();
  }

  void onAnimationTimerTimeout() {
    m_currentTime +=
        m_animationTimer->interval() / 1000.0f; // Convert ms to seconds
    if (m_currentTime > m_animationDuration) {
      m_currentTime = 0.0f; // Loop back to start
    }
    m_canvas->setCurrentTime(m_currentTime);
    m_canvas->update();
    m_canvas->scene().scriptSystem.tick(
        m_animationTimer->interval() / 1000.0f,
        m_currentTime); // Pass delta time and current time
    m_timelineSlider->setValue(static_cast<int>(m_currentTime * 100));
    updateTimeDisplay();
  }

  void onTimelineSliderMoved(int value) {
    m_currentTime = value / 100.0f;
    m_canvas->setCurrentTime(m_currentTime);
    m_canvas->update();
    updateTimeDisplay();
  }

  void updateTimeDisplay() {
    m_timeDisplayLabel->setText(QString("%1s / %2s")
                                    .arg(m_currentTime, 0, 'f', 2)
                                    .arg(m_animationDuration, 0, 'f', 2));
  }

  void onSceneSelectionChanged(const QItemSelection &selected,
                               const QItemSelection &deselected) {
    clearLayout(m_propsLayout);

    auto indexes = selected.indexes();
    if (indexes.isEmpty()) {
      m_canvas->setSelectedEntity(kInvalidEntity);
      m_canvas->update();
      return;
    }

    Entity e = m_sceneModel->getEntity(indexes.first());
    m_canvas->setSelectedEntity(e);
    m_canvas->update();

    if (auto *c = m_canvas->scene().reg.get<NameComponent>(e)) {
      auto *nameGroup = new QGroupBox(tr("Object"));
      auto *nameLayout = new QFormLayout(nameGroup);
      auto *nameEdit = new QLineEdit(QString::fromStdString(c->name));
      nameLayout->addRow(tr("Name"), nameEdit);
      connect(nameEdit, &QLineEdit::textChanged,
              [this, e](const QString &text) {
                if (auto *c = m_canvas->scene().reg.get<NameComponent>(e)) {
                  c->name = text.toStdString();
                  m_sceneModel->refresh();
                }
              });
      m_propsLayout->addRow(nameGroup);
    }

    if (auto *c = m_canvas->scene().reg.get<ShapeComponent>(e)) {
      auto *shapeGroup = new QGroupBox(tr("Shape"));
      auto *shapeLayout = new QFormLayout(shapeGroup);
      shapeLayout->addRow(tr("Shape"),
                          new QLabel(ShapeComponent::toString(c->kind)));

      QVariantMap shapeProps = m_sceneModel
                                   ->data(m_sceneModel->indexOfEntity(e),
                                          SceneModel::ShapePropertiesRole)
                                   .toMap();

      if (c->kind == ShapeComponent::Kind::Rectangle) {
        auto *widthSpin = new QDoubleSpinBox();
        widthSpin->setRange(0.1, 10000.0);
        widthSpin->setValue(shapeProps["width"].toFloat());
        connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      rectProps->width = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Width"), widthSpin);

        auto *heightSpin = new QDoubleSpinBox();
        heightSpin->setRange(0.1, 10000.0);
        heightSpin->setValue(shapeProps["height"].toFloat());
        connect(heightSpin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      rectProps->height = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Height"), heightSpin);

        auto *gridXStepSpin = new QDoubleSpinBox();
        gridXStepSpin->setRange(0.1, 1000.0);
        gridXStepSpin->setValue(shapeProps["grid_xstep"].toFloat());
        connect(gridXStepSpin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      rectProps->grid_xstep = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Grid X Step"), gridXStepSpin);

        auto *gridYStepSpin = new QDoubleSpinBox();
        gridYStepSpin->setRange(0.1, 1000.0);
        gridYStepSpin->setValue(shapeProps["grid_ystep"].toFloat());
        connect(gridYStepSpin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      rectProps->grid_ystep = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Grid Y Step"), gridYStepSpin);

      } else if (c->kind == ShapeComponent::Kind::Circle) {
        auto *radiusSpin = new QDoubleSpinBox();
        radiusSpin->setRange(0.1, 10000.0);
        radiusSpin->setValue(shapeProps["radius"].toFloat());
        connect(
            radiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e](double val) {
              if (auto *shapeComp =
                      m_canvas->scene().reg.get<ShapeComponent>(e)) {
                if (auto *circleProps =
                        std::get_if<CircleProperties>(&shapeComp->properties)) {
                  circleProps->radius = val;
                  m_canvas->update();
                }
              }
            });
        shapeLayout->addRow(tr("Radius"), radiusSpin);
      } else if (c->kind == ShapeComponent::Kind::Line) {
        auto *x1Spin = new QDoubleSpinBox();
        x1Spin->setRange(-10000.0, 10000.0);
        x1Spin->setValue(shapeProps["x1"].toFloat());
        connect(x1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      lineProps->x1 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X1"), x1Spin);

        auto *y1Spin = new QDoubleSpinBox();
        y1Spin->setRange(-10000.0, 10000.0);
        y1Spin->setValue(shapeProps["y1"].toFloat());
        connect(y1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      lineProps->y1 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y1"), y1Spin);

        auto *x2Spin = new QDoubleSpinBox();
        x2Spin->setRange(-10000.0, 10000.0);
        x2Spin->setValue(shapeProps["x2"].toFloat());
        connect(x2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      lineProps->x2 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X2"), x2Spin);

        auto *y2Spin = new QDoubleSpinBox();
        y2Spin->setRange(-10000.0, 10000.0);
        y2Spin->setValue(shapeProps["y2"].toFloat());
        connect(y2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      lineProps->y2 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y2"), y2Spin);
      } else if (c->kind == ShapeComponent::Kind::Bezier) {
        auto *x1Spin = new QDoubleSpinBox();
        x1Spin->setRange(-10000.0, 10000.0);
        x1Spin->setValue(shapeProps["x1"].toFloat());
        connect(x1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->x1 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X1"), x1Spin);

        auto *y1Spin = new QDoubleSpinBox();
        y1Spin->setRange(-10000.0, 10000.0);
        y1Spin->setValue(shapeProps["y1"].toFloat());
        connect(y1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->y1 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y1"), y1Spin);

        auto *cx1Spin = new QDoubleSpinBox();
        cx1Spin->setRange(-10000.0, 10000.0);
        cx1Spin->setValue(shapeProps["cx1"].toFloat());
        connect(cx1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->cx1 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CX1"), cx1Spin);

        auto *cy1Spin = new QDoubleSpinBox();
        cy1Spin->setRange(-10000.0, 10000.0);
        cy1Spin->setValue(shapeProps["cy1"].toFloat());
        connect(cy1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->cy1 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CY1"), cy1Spin);

        auto *cx2Spin = new QDoubleSpinBox();
        cx2Spin->setRange(-10000.0, 10000.0);
        cx2Spin->setValue(shapeProps["cx2"].toFloat());
        connect(cx2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->cx2 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CX2"), cx2Spin);

        auto *cy2Spin = new QDoubleSpinBox();
        cy2Spin->setRange(-10000.0, 10000.0);
        cy2Spin->setValue(shapeProps["cy2"].toFloat());
        connect(cy2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->cy2 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CY2"), cy2Spin);

        auto *x2Spin = new QDoubleSpinBox();
        x2Spin->setRange(-10000.0, 10000.0);
        x2Spin->setValue(shapeProps["x2"].toFloat());
        connect(x2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->x2 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X2"), x2Spin);

        auto *y2Spin = new QDoubleSpinBox();
        y2Spin->setRange(-10000.0, 10000.0);
        y2Spin->setValue(shapeProps["y2"].toFloat());
        connect(y2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      bezierProps->y2 = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y2"), y2Spin);
      } else if (c->kind == ShapeComponent::Kind::Text) {
        auto *textEdit = new QLineEdit(shapeProps["text"].toString());
        connect(
            textEdit, &QLineEdit::textChanged, [this, e](const QString &val) {
              if (auto *shapeComp =
                      m_canvas->scene().reg.get<ShapeComponent>(e)) {
                if (auto *textProps =
                        std::get_if<TextProperties>(&shapeComp->properties)) {
                  textProps->text = val;
                  m_canvas->update();
                }
              }
            });
        shapeLayout->addRow(tr("Text"), textEdit);

        auto *fontSizeSpin = new QDoubleSpinBox();
        fontSizeSpin->setRange(1.0, 500.0);
        fontSizeSpin->setValue(shapeProps["fontSize"].toFloat());
        connect(
            fontSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e](double val) {
              if (auto *shapeComp =
                      m_canvas->scene().reg.get<ShapeComponent>(e)) {
                if (auto *textProps =
                        std::get_if<TextProperties>(&shapeComp->properties)) {
                  textProps->fontSize = val;
                  m_canvas->update();
                }
              }
            });
        shapeLayout->addRow(tr("Font Size"), fontSizeSpin);

        auto *fontFamilyEdit =
            new QLineEdit(shapeProps["fontFamily"].toString());
        connect(fontFamilyEdit, &QLineEdit::textChanged,
                [this, e](const QString &val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *textProps = std::get_if<TextProperties>(
                            &shapeComp->properties)) {
                      textProps->fontFamily = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Font Family"), fontFamilyEdit);
      } else if (c->kind == ShapeComponent::Kind::Image) {
        auto *filePathEdit = new QLineEdit(shapeProps["filePath"].toString());
        connect(filePathEdit, &QLineEdit::textChanged,
                [this, e](const QString &val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *imageProps = std::get_if<ImageProperties>(
                            &shapeComp->properties)) {
                      imageProps->filePath = val;
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("File Path"), filePathEdit);

        auto *browseButton = new QPushButton("Browse...");
        connect(browseButton, &QPushButton::clicked, [this, e, filePathEdit]() {
          QString filePath = QFileDialog::getOpenFileName(
              this, tr("Select Image"), QString(),
              tr("Image Files (*.png *.jpg *.bmp *.gif)"));
          if (!filePath.isEmpty()) {
            filePathEdit->setText(filePath);
          }
        });
        shapeLayout->addRow(browseButton);
      }
      m_propsLayout->addRow(shapeGroup);

      if (auto *c = m_canvas->scene().reg.get<TransformComponent>(e)) {
        auto *transformGroup = new QGroupBox(tr("Transform"));
        auto *transformLayout = new QFormLayout(transformGroup);

        auto *xSpin = new QSpinBox();
        xSpin->setRange(-1000, 1000);
        xSpin->setValue(c->x);
        connect(xSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                [this, e](int val) {
                  if (auto *c =
                          m_canvas->scene().reg.get<TransformComponent>(e)) {
                    c->x = val;
                    m_canvas->update();
                  }
                });

        auto *ySpin = new QSpinBox();
        ySpin->setRange(-1000, 1000);
        ySpin->setValue(c->y);
        connect(ySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                [this, e](int val) {
                  if (auto *c =
                          m_canvas->scene().reg.get<TransformComponent>(e)) {
                    c->y = val;
                    m_canvas->update();
                  }
                });

        auto *posLayout = new QHBoxLayout();
        posLayout->addWidget(xSpin);
        posLayout->addWidget(ySpin);
        transformLayout->addRow(tr("Position"), posLayout);

        auto *rotationSpin = new QDoubleSpinBox();
        rotationSpin->setRange(-1000000, 1000000);
        rotationSpin->setValue(c->rotation * 180 / M_PI);
        connect(
            rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e](double val) {
              if (auto *c = m_canvas->scene().reg.get<TransformComponent>(e)) {
                c->rotation = val * M_PI / 180.0;
                m_canvas->update();
              }
            });
        transformLayout->addRow(tr("Rotation"), rotationSpin);

        auto *sxSpin = new QDoubleSpinBox();
        sxSpin->setRange(0.01, 100.0);
        sxSpin->setValue(c->sx);
        connect(sxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *c =
                          m_canvas->scene().reg.get<TransformComponent>(e)) {
                    c->sx = val;
                    m_canvas->update();
                  }
                });
        transformLayout->addRow(tr("Scale X"), sxSpin);

        auto *sySpin = new QDoubleSpinBox();
        sySpin->setRange(0.01, 100.0);
        sySpin->setValue(c->sy);
        connect(sySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *c =
                          m_canvas->scene().reg.get<TransformComponent>(e)) {
                    c->sy = val;
                    m_canvas->update();
                  }
                });
        transformLayout->addRow(tr("Scale Y"), sySpin);
        m_propsLayout->addRow(transformGroup);
      }

      if (auto *c = m_canvas->scene().reg.get<MaterialComponent>(e)) {
        auto *materialGroup = new QGroupBox(tr("Material"));
        auto *materialLayout = new QFormLayout(materialGroup);

        // Color
        auto *colorButton = new QPushButton("Change Color");
        QPalette pal = colorButton->palette();
        pal.setColor(QPalette::Button, QColor::fromRgba(c->color));
        colorButton->setAutoFillBackground(true);
        colorButton->setPalette(pal);
        colorButton->update();
        connect(colorButton, &QPushButton::clicked, [this, e, colorButton]() {
          QColor initialColor = QColor::fromRgba(
              m_canvas->scene().reg.get<MaterialComponent>(e)->color);
          QColor newColor = QColorDialog::getColor(initialColor, this);
          if (newColor.isValid()) {
            if (auto *mc = m_canvas->scene().reg.get<MaterialComponent>(e)) {
              mc->color = newColor.rgba();
              QPalette pal = colorButton->palette();
              pal.setColor(QPalette::Button, newColor);
              colorButton->setPalette(pal);
              colorButton->update();
              m_canvas->update();
            }
          }
        });
        materialLayout->addRow(tr("Color"), colorButton);

        // Is Filled
        auto *isFilledCheckBox = new QCheckBox();
        isFilledCheckBox->setChecked(c->isFilled);
        connect(
            isFilledCheckBox, &QCheckBox::stateChanged, [this, e](int state) {
              if (auto *mc = m_canvas->scene().reg.get<MaterialComponent>(e)) {
                mc->isFilled = state == Qt::Checked;
                m_canvas->update();
              }
            });
        materialLayout->addRow(tr("Filled"), isFilledCheckBox);

        // Is Stroked
        auto *isStrokedCheckBox = new QCheckBox();
        isStrokedCheckBox->setChecked(c->isStroked);
        connect(
            isStrokedCheckBox, &QCheckBox::stateChanged, [this, e](int state) {
              if (auto *mc = m_canvas->scene().reg.get<MaterialComponent>(e)) {
                mc->isStroked = state == Qt::Checked;
                m_canvas->update();
              }
            });
        materialLayout->addRow(tr("Stroked"), isStrokedCheckBox);

        // Stroke Width
        auto *strokeWidthSpinBox = new QDoubleSpinBox();
        strokeWidthSpinBox->setRange(0.0, 100.0);
        strokeWidthSpinBox->setValue(c->strokeWidth);
        connect(strokeWidthSpinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e](double val) {
                  if (auto *mc =
                          m_canvas->scene().reg.get<MaterialComponent>(e)) {
                    mc->strokeWidth = val;
                    m_canvas->update();
                  }
                });
        materialLayout->addRow(tr("Stroke Width"), strokeWidthSpinBox);

        // Anti-aliased
        auto *antiAliasedCheckBox = new QCheckBox();
        antiAliasedCheckBox->setChecked(c->antiAliased);
        connect(antiAliasedCheckBox, &QCheckBox::stateChanged,
                [this, e](int state) {
                  if (auto *mc =
                          m_canvas->scene().reg.get<MaterialComponent>(e)) {
                    mc->antiAliased = state == Qt::Checked;
                    m_canvas->update();
                  }
                });
        materialLayout->addRow(tr("Anti-aliased"), antiAliasedCheckBox);

        m_propsLayout->addRow(materialGroup);
      }

      if (auto *c = m_canvas->scene().reg.get<AnimationComponent>(e)) {
        auto *animationGroup = new QGroupBox(tr("Animation"));
        auto *animationLayout = new QFormLayout(animationGroup);

        auto *entryTimeSpin = new QDoubleSpinBox();
        entryTimeSpin->setRange(0.0, 1000.0);
        entryTimeSpin->setSingleStep(0.1);
        entryTimeSpin->setValue(c->entryTime);
        connect(
            entryTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e](double val) {
              if (auto *ac = m_canvas->scene().reg.get<AnimationComponent>(e)) {
                ac->entryTime = val;
                m_canvas->update();
              }
            });
        animationLayout->addRow(tr("Entry Time"), entryTimeSpin);

        auto *exitTimeSpin = new QDoubleSpinBox();
        exitTimeSpin->setRange(0.0, 1000.0);
        exitTimeSpin->setSingleStep(0.1);
        exitTimeSpin->setValue(c->exitTime);
        connect(
            exitTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e](double val) {
              if (auto *ac = m_canvas->scene().reg.get<AnimationComponent>(e)) {
                ac->exitTime = val;
                m_canvas->update();
              }
            });
        animationLayout->addRow(tr("Exit Time"), exitTimeSpin);

        m_propsLayout->addRow(animationGroup);
      }

      if (auto *c = m_canvas->scene().reg.get<ScriptComponent>(e)) {
        auto *scriptGroup = new QGroupBox(tr("Script"));
        auto *scriptLayout = new QFormLayout(scriptGroup);

        QVariantMap scriptProps = m_sceneModel
                                      ->data(m_sceneModel->indexOfEntity(e),
                                             SceneModel::ScriptPropertiesRole)
                                      .toMap();

        auto *scriptPathEdit =
            new QLineEdit(scriptProps["scriptPath"].toString());
        connect(scriptPathEdit, &QLineEdit::textChanged,
                [this, e](const QString &val) {
                  if (auto *sc =
                          m_canvas->scene().reg.get<ScriptComponent>(e)) {
                    sc->scriptPath = val.toStdString();
                  }
                });
        scriptLayout->addRow(tr("Path"), scriptPathEdit);

        auto *startFunctionEdit =
            new QLineEdit(scriptProps["startFunction"].toString());
        connect(startFunctionEdit, &QLineEdit::textChanged,
                [this, e](const QString &val) {
                  if (auto *sc =
                          m_canvas->scene().reg.get<ScriptComponent>(e)) {
                    sc->startFunction = val.toStdString();
                  }
                });
        scriptLayout->addRow(tr("Start Function"), startFunctionEdit);

        auto *updateFunctionEdit =
            new QLineEdit(scriptProps["updateFunction"].toString());
        connect(updateFunctionEdit, &QLineEdit::textChanged,
                [this, e](const QString &val) {
                  if (auto *sc =
                          m_canvas->scene().reg.get<ScriptComponent>(e)) {
                    sc->updateFunction = val.toStdString();
                  }
                });
        scriptLayout->addRow(tr("Update Function"), updateFunctionEdit);

        auto *destroyFunctionEdit =
            new QLineEdit(scriptProps["destroyFunction"].toString());
        connect(destroyFunctionEdit, &QLineEdit::textChanged,
                [this, e](const QString &val) {
                  if (auto *sc =
                          m_canvas->scene().reg.get<ScriptComponent>(e)) {
                    sc->destroyFunction = val.toStdString();
                  }
                });
        scriptLayout->addRow(tr("Destroy Function"), destroyFunctionEdit);

        m_propsLayout->addRow(scriptGroup);
      }
    }
  }
private slots:
  void onTransformChanged(Entity entity) {
    // Get the currently selected index from the scene tree
    QModelIndexList selectedIndexes =
        m_sceneTree->selectionModel()->selectedIndexes();
    if (!selectedIndexes.isEmpty()) {
      Entity currentSelectedEntity =
          m_sceneModel->getEntity(selectedIndexes.first());
      if (currentSelectedEntity == entity) {
        // If the transformed entity is the one currently selected in the
        // tree, force a refresh of the properties panel by directly calling
        // onSceneSelectionChanged with the current selection.
        onSceneSelectionChanged(m_sceneTree->selectionModel()->selection(),
                                QItemSelection());
      }
    }
  }

  void onCanvasSelectionChanged(Entity entity) {
    if (entity == kInvalidEntity) {
      m_sceneTree->selectionModel()->clearSelection();
      return;
    }

    QModelIndex index = m_sceneModel->indexOfEntity(entity);
    if (index.isValid()) {
      m_sceneTree->selectionModel()->select(
          index, QItemSelectionModel::ClearAndSelect);
    }
  }

private:
  void clearLayout(QLayout *layout) {
    if (!layout)
      return;
    while (layout->count() > 0) {
      QLayoutItem *item = layout->takeAt(0);
      if (QWidget *widget = item->widget()) {
        widget->deleteLater();
      }
      if (QLayout *childLayout = item->layout()) {
        clearLayout(childLayout); // Recursively clear child layouts
      }
      delete item;
    }
  }

  void resetScene() {
    m_canvas->scene().reg = m_initialRegistry; // Copy back the initial state
    m_sceneModel->refresh(); // Refresh the model to reflect changes
    // Re-select the entity to update properties panel if needed
    if (!m_sceneTree->selectionModel()->selectedIndexes().isEmpty()) {
      Entity currentSelectedEntity = m_sceneModel->getEntity(
          m_sceneTree->selectionModel()->selectedIndexes().first());
      onSceneSelectionChanged(m_sceneTree->selectionModel()->selection(),
                              QItemSelection());
    }
  }

  SkiaCanvasWidget *m_canvas = nullptr;
  SceneModel *m_sceneModel = nullptr;
  QTreeView *m_sceneTree = nullptr;
  QFormLayout *m_propsLayout = nullptr;

  Registry m_initialRegistry; // Store the initial state of the registry

  QPushButton *m_playPauseButton = nullptr;
  QPushButton *m_stopResetButton = nullptr;
  QLabel *m_timeDisplayLabel = nullptr;
  QSlider *m_timelineSlider = nullptr;
  QTimer *m_animationTimer = nullptr;
  bool m_isPlaying = false;
  float m_currentTime = 0.0f;
  float m_animationDuration = 5.0f; // Default animation duration
};
