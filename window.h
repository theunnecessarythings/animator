#pragma once

#include "canvas.h"
#include "scene_model.h"
#include "toolbox.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeView>
#include <QUndoStack>
#include <QVBoxLayout>

#include <QSlider>
#include <QTimer>

#include "commands.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent), m_undoStack(new QUndoStack(this)) {
    m_canvas = new SkiaCanvasWidget(this);
    setCentralWidget(m_canvas);

    createMenus();
    createToolbox();
    createSceneDock();
    createPropertiesDock();
    createTimelineDock();

    setWindowTitle(tr("Animation Studio"));
  }

  SkiaCanvasWidget *canvas() const { return m_canvas; }
  SceneModel *sceneModel() const { return m_sceneModel; }
  QTreeView *sceneTree() const { return m_sceneTree; }

  void setInitialRegistry(const Registry &reg) { m_initialRegistry = reg; }

private:
  void createMenus() {
    // --- File -----------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), this, &MainWindow::onNewFile);
    fileMenu->addAction(tr("&Open…"), this, &MainWindow::onOpenFile);
    fileMenu->addAction(tr("&Save"), this, &MainWindow::onSaveFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), qApp, &QCoreApplication::quit);

    // --- Edit -----------------------------------------------------------
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction *undoAction =
        editMenu->addAction(tr("&Undo"), m_undoStack, &QUndoStack::undo);
    undoAction->setShortcut(QKeySequence::Undo);
    QAction *redoAction =
        editMenu->addAction(tr("&Redo"), m_undoStack, &QUndoStack::redo);
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addSeparator();
    QAction *cutAction =
        editMenu->addAction(tr("Cu&t"), this, &MainWindow::onCut);
    cutAction->setShortcut(QKeySequence::Cut);
    QAction *copyAction =
        editMenu->addAction(tr("&Copy"), this, &MainWindow::onCopy);
    copyAction->setShortcut(QKeySequence::Copy);
    QAction *pasteAction =
        editMenu->addAction(tr("&Paste"), this, &MainWindow::onPaste);
    pasteAction->setShortcut(QKeySequence::Paste);
    editMenu->addSeparator();
    QAction *deleteAction =
        editMenu->addAction(tr("&Delete"), this, &MainWindow::onDelete);
    deleteAction->setShortcut(QKeySequence::Delete);

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
    connect(m_canvas, &SkiaCanvasWidget::transformationCompleted, this,
            &MainWindow::onTransformationCompleted);
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
      resetScene();         // Reset the scene when animation loops
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
                               const QItemSelection & /*deselected*/) {
    clearLayout(m_propsLayout);

    m_selectedEntities.clear();
    auto indexes = selected.indexes();
    if (indexes.isEmpty()) {
      m_canvas->setSelectedEntity(kInvalidEntity);
      m_canvas->update();
      return;
    }

    // For now, only handle the first selected entity for properties panel
    // Multi-selection for properties will require a different approach
    Entity firstSelectedEntity = m_sceneModel->getEntity(indexes.first());
    m_canvas->setSelectedEntity(firstSelectedEntity);
    m_canvas->update();

    for (const auto &index : indexes) {
      m_selectedEntities.append(m_sceneModel->getEntity(index));
    }

    Entity e = firstSelectedEntity;

    if (auto *c = m_canvas->scene().reg.get<NameComponent>(e)) {
      auto *nameGroup = new QGroupBox(tr("Object"));
      auto *nameLayout = new QFormLayout(nameGroup);
      auto *nameEdit = new QLineEdit(QString::fromStdString(c->name));
      nameLayout->addRow(tr("Name"), nameEdit);
      connect(nameEdit, &QLineEdit::textChanged,
              [this, e, nameEdit, c](const QString &text) {
                if (nameEdit->property("textChangedByCode").toBool()) return; // Avoid infinite loop
                std::string oldName = c->name;
                std::string newName = text.toStdString();
                if (oldName != newName) {
                  m_undoStack->push(new ChangeNameCommand(this, e, oldName, newName));
                }
                c->name = newName;
                m_sceneModel->refresh();
              });
      // Set a property to indicate that the text change is programmatic
      connect(nameEdit, &QLineEdit::textEdited, [nameEdit](){
        nameEdit->setProperty("textChangedByCode", false);
      });
      nameEdit->setProperty("textChangedByCode", true);
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
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      rectProps->width = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
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
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      rectProps->height = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
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
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      rectProps->grid_xstep = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
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
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *rectProps = std::get_if<RectangleProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      rectProps->grid_ystep = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
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
            [this, e, c](double val) {
              if (auto *shapeComp =
                      m_canvas->scene().reg.get<ShapeComponent>(e)) {
                if (auto *circleProps =
                        std::get_if<CircleProperties>(&shapeComp->properties)) {
                  QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                  circleProps->radius = val;
                  QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                  m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
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
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      lineProps->x1 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X1"), x1Spin);

        auto *y1Spin = new QDoubleSpinBox();
        y1Spin->setRange(-10000.0, 10000.0);
        y1Spin->setValue(shapeProps["y1"].toFloat());
        connect(y1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      lineProps->y1 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y1"), y1Spin);

        auto *x2Spin = new QDoubleSpinBox();
        x2Spin->setRange(-10000.0, 10000.0);
        x2Spin->setValue(shapeProps["x2"].toFloat());
        connect(x2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      lineProps->x2 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X2"), x2Spin);

        auto *y2Spin = new QDoubleSpinBox();
        y2Spin->setRange(-10000.0, 10000.0);
        y2Spin->setValue(shapeProps["y2"].toFloat());
        connect(y2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *lineProps = std::get_if<LineProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      lineProps->y2 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
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
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->x1 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X1"), x1Spin);

        auto *y1Spin = new QDoubleSpinBox();
        y1Spin->setRange(-10000.0, 10000.0);
        y1Spin->setValue(shapeProps["y1"].toFloat());
        connect(y1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->y1 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y1"), y1Spin);

        auto *cx1Spin = new QDoubleSpinBox();
        cx1Spin->setRange(-10000.0, 10000.0);
        cx1Spin->setValue(shapeProps["cx1"].toFloat());
        connect(cx1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->cx1 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CX1"), cx1Spin);

        auto *cy1Spin = new QDoubleSpinBox();
        cy1Spin->setRange(-10000.0, 10000.0);
        cy1Spin->setValue(shapeProps["cy1"].toFloat());
        connect(cy1Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->cy1 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CY1"), cy1Spin);

        auto *cx2Spin = new QDoubleSpinBox();
        cx2Spin->setRange(-10000.0, 10000.0);
        cx2Spin->setValue(shapeProps["cx2"].toFloat());
        connect(cx2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->cx2 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CX2"), cx2Spin);

        auto *cy2Spin = new QDoubleSpinBox();
        cy2Spin->setRange(-10000.0, 10000.0);
        cy2Spin->setValue(shapeProps["cy2"].toFloat());
        connect(cy2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->cy2 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("CY2"), cy2Spin);

        auto *x2Spin = new QDoubleSpinBox();
        x2Spin->setRange(-10000.0, 10000.0);
        x2Spin->setValue(shapeProps["x2"].toFloat());
        connect(x2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->x2 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("X2"), x2Spin);

        auto *y2Spin = new QDoubleSpinBox();
        y2Spin->setRange(-10000.0, 10000.0);
        y2Spin->setValue(shapeProps["y2"].toFloat());
        connect(y2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *bezierProps = std::get_if<BezierProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      bezierProps->y2 = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        shapeLayout->addRow(tr("Y2"), y2Spin);
      } else if (c->kind == ShapeComponent::Kind::Text) {
        auto *textEdit = new QLineEdit(shapeProps["text"].toString());
        connect(
            textEdit, &QLineEdit::textChanged, [this, e, c, textEdit](const QString &val) {
              if (textEdit->property("textChangedByCode").toBool()) return;
              if (auto *shapeComp =
                      m_canvas->scene().reg.get<ShapeComponent>(e)) {
                if (auto *textProps =
                        std::get_if<TextProperties>(&shapeComp->properties)) {
                  QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                  textProps->text = val;
                  QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                  m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                  m_canvas->update();
                }
              }
            });
        connect(textEdit, &QLineEdit::textEdited, [textEdit](){
          textEdit->setProperty("textChangedByCode", false);
        });
        textEdit->setProperty("textChangedByCode", true);
        shapeLayout->addRow(tr("Text"), textEdit);

        auto *fontSizeSpin = new QDoubleSpinBox();
        fontSizeSpin->setRange(1.0, 500.0);
        fontSizeSpin->setValue(shapeProps["fontSize"].toFloat());
        connect(
            fontSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e, c](double val) {
              if (auto *shapeComp =
                      m_canvas->scene().reg.get<ShapeComponent>(e)) {
                if (auto *textProps =
                        std::get_if<TextProperties>(&shapeComp->properties)) {
                  QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                  textProps->fontSize = val;
                  QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                  m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                  m_canvas->update();
                }
              }
            });
        shapeLayout->addRow(tr("Font Size"), fontSizeSpin);

        auto *fontFamilyEdit =
            new QLineEdit(shapeProps["fontFamily"].toString());
        connect(fontFamilyEdit, &QLineEdit::textChanged,
                [this, e, c, fontFamilyEdit](const QString &val) {
                  if (fontFamilyEdit->property("textChangedByCode").toBool()) return;
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *textProps = std::get_if<TextProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      textProps->fontFamily = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        connect(fontFamilyEdit, &QLineEdit::textEdited, [fontFamilyEdit](){
          fontFamilyEdit->setProperty("textChangedByCode", false);
        });
        fontFamilyEdit->setProperty("textChangedByCode", true);
        shapeLayout->addRow(tr("Font Family"), fontFamilyEdit);
      } else if (c->kind == ShapeComponent::Kind::Image) {
        auto *filePathEdit = new QLineEdit(shapeProps["filePath"].toString());
        connect(filePathEdit, &QLineEdit::textChanged,
                [this, e, c, filePathEdit](const QString &val) {
                  if (filePathEdit->property("textChangedByCode").toBool()) return;
                  if (auto *shapeComp =
                          m_canvas->scene().reg.get<ShapeComponent>(e)) {
                    if (auto *imageProps = std::get_if<ImageProperties>(
                            &shapeComp->properties)) {
                      QJsonObject oldProps = shapeComp->getProperties().toJsonObject();
                      imageProps->filePath = val;
                      QJsonObject newProps = shapeComp->getProperties().toJsonObject();
                      m_undoStack->push(new ChangeShapePropertyCommand(this, e, c->kind, oldProps, newProps));
                      m_canvas->update();
                    }
                  }
                });
        connect(filePathEdit, &QLineEdit::textEdited, [filePathEdit](){
          filePathEdit->setProperty("textChangedByCode", false);
        });
        filePathEdit->setProperty("textChangedByCode", true);
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
                [this, e, c](int val) {
                  float oldX = c->x;
                  float oldY = c->y;
                  float oldRotation = c->rotation;
                  float oldSx = c->sx;
                  float oldSy = c->sy;
                  float newX = val;
                  if (oldX != newX) {
                    m_undoStack->push(new ChangeTransformCommand(this, e, oldX, oldY, oldRotation, oldSx, oldSy, newX, oldY, oldRotation, oldSx, oldSy));
                  }
                  c->x = newX;
                  m_canvas->update();
                });

        auto *ySpin = new QSpinBox();
        ySpin->setRange(-1000, 1000);
        ySpin->setValue(c->y);
        connect(ySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                [this, e, c](int val) {
                  float oldX = c->x;
                  float oldY = c->y;
                  float oldRotation = c->rotation;
                  float oldSx = c->sx;
                  float oldSy = c->sy;
                  float newY = val;
                  if (oldY != newY) {
                    m_undoStack->push(new ChangeTransformCommand(this, e, oldX, oldY, oldRotation, oldSx, oldSy, oldX, newY, oldRotation, oldSx, oldSy));
                  }
                  c->y = newY;
                  m_canvas->update();
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
            [this, e, c](double val) {
              float oldX = c->x;
              float oldY = c->y;
              float oldRotation = c->rotation;
              float oldSx = c->sx;
              float oldSy = c->sy;
              float newRotation = val * M_PI / 180.0;
              if (oldRotation != newRotation) {
                m_undoStack->push(new ChangeTransformCommand(this, e, oldX, oldY, oldRotation, oldSx, oldSy, oldX, oldY, newRotation, oldSx, oldSy));
              }
              c->rotation = newRotation;
              m_canvas->update();
            });
        transformLayout->addRow(tr("Rotation"), rotationSpin);

        auto *sxSpin = new QDoubleSpinBox();
        sxSpin->setRange(0.01, 100.0);
        sxSpin->setValue(c->sx);
        connect(sxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  float oldX = c->x;
                  float oldY = c->y;
                  float oldRotation = c->rotation;
                  float oldSx = c->sx;
                  float oldSy = c->sy;
                  float newSx = val;
                  if (oldSx != newSx) {
                    m_undoStack->push(new ChangeTransformCommand(this, e, oldX, oldY, oldRotation, oldSx, oldSy, oldX, oldY, oldRotation, newSx, oldSy));
                  }
                  c->sx = newSx;
                  m_canvas->update();
                });
        transformLayout->addRow(tr("Scale X"), sxSpin);

        auto *sySpin = new QDoubleSpinBox();
        sySpin->setRange(0.01, 100.0);
        sySpin->setValue(c->sy);
        connect(sySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  float oldX = c->x;
                  float oldY = c->y;
                  float oldRotation = c->rotation;
                  float oldSx = c->sx;
                  float oldSy = c->sy;
                  float newSy = val;
                  if (oldSy != newSy) {
                    m_undoStack->push(new ChangeTransformCommand(this, e, oldX, oldY, oldRotation, oldSx, oldSy, oldX, oldY, oldRotation, oldSx, newSy));
                  }
                  c->sy = newSy;
                  m_canvas->update();
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
        connect(colorButton, &QPushButton::clicked, [this, e, colorButton, c]() {
          QColor initialColor = QColor::fromRgba(c->color);
          QColor newColor = QColorDialog::getColor(initialColor, this);
          if (newColor.isValid()) {
            SkColor oldColor = c->color;
            bool oldIsFilled = c->isFilled;
            bool oldIsStroked = c->isStroked;
            float oldStrokeWidth = c->strokeWidth;
            bool oldAntiAliased = c->antiAliased;

            SkColor newSkColor = newColor.rgba();
            if (oldColor != newSkColor) {
              m_undoStack->push(new ChangeMaterialCommand(this, e, oldColor, oldIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased, newSkColor, oldIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased));
            }
            c->color = newSkColor;
            QPalette pal = colorButton->palette();
            pal.setColor(QPalette::Button, newColor);
            colorButton->setPalette(pal);
            colorButton->update();
            m_canvas->update();
          }
        });
        materialLayout->addRow(tr("Color"), colorButton);

        // Is Filled
        auto *isFilledCheckBox = new QCheckBox();
        isFilledCheckBox->setChecked(c->isFilled);
        connect(
            isFilledCheckBox, &QCheckBox::stateChanged, [this, e, c](int state) {
              SkColor oldColor = c->color;
              bool oldIsFilled = c->isFilled;
              bool oldIsStroked = c->isStroked;
              float oldStrokeWidth = c->strokeWidth;
              bool oldAntiAliased = c->antiAliased;

              bool newIsFilled = state == Qt::Checked;
              if (oldIsFilled != newIsFilled) {
                m_undoStack->push(new ChangeMaterialCommand(this, e, oldColor, oldIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased, oldColor, newIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased));
              }
              c->isFilled = newIsFilled;
              m_canvas->update();
            });
        materialLayout->addRow(tr("Filled"), isFilledCheckBox);

        // Is Stroked
        auto *isStrokedCheckBox = new QCheckBox();
        isStrokedCheckBox->setChecked(c->isStroked);
        connect(
            isStrokedCheckBox, &QCheckBox::stateChanged, [this, e, c](int state) {
              SkColor oldColor = c->color;
              bool oldIsFilled = c->isFilled;
              bool oldIsStroked = c->isStroked;
              float oldStrokeWidth = c->strokeWidth;
              bool oldAntiAliased = c->antiAliased;

              bool newIsStroked = state == Qt::Checked;
              if (oldIsStroked != newIsStroked) {
                m_undoStack->push(new ChangeMaterialCommand(this, e, oldColor, oldIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased, oldColor, oldIsFilled, newIsStroked, oldStrokeWidth, oldAntiAliased));
              }
              c->isStroked = newIsStroked;
              m_canvas->update();
            });
        materialLayout->addRow(tr("Stroked"), isStrokedCheckBox);

        // Stroke Width
        auto *strokeWidthSpinBox = new QDoubleSpinBox();
        strokeWidthSpinBox->setRange(0.0, 100.0);
        strokeWidthSpinBox->setValue(c->strokeWidth);
        connect(strokeWidthSpinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, e, c](double val) {
                  SkColor oldColor = c->color;
                  bool oldIsFilled = c->isFilled;
                  bool oldIsStroked = c->isStroked;
                  float oldStrokeWidth = c->strokeWidth;
                  bool oldAntiAliased = c->antiAliased;

                  float newStrokeWidth = val;
                  if (oldStrokeWidth != newStrokeWidth) {
                    m_undoStack->push(new ChangeMaterialCommand(this, e, oldColor, oldIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased, oldColor, oldIsFilled, oldIsStroked, newStrokeWidth, oldAntiAliased));
                  }
                  c->strokeWidth = newStrokeWidth;
                  m_canvas->update();
                });
        materialLayout->addRow(tr("Stroke Width"), strokeWidthSpinBox);

        // Anti-aliased
        auto *antiAliasedCheckBox = new QCheckBox();
        antiAliasedCheckBox->setChecked(c->antiAliased);
        connect(antiAliasedCheckBox, &QCheckBox::stateChanged,
                [this, e, c](int state) {
                  SkColor oldColor = c->color;
                  bool oldIsFilled = c->isFilled;
                  bool oldIsStroked = c->isStroked;
                  float oldStrokeWidth = c->strokeWidth;
                  bool oldAntiAliased = c->antiAliased;

                  bool newAntiAliased = state == Qt::Checked;
                  if (oldAntiAliased != newAntiAliased) {
                    m_undoStack->push(new ChangeMaterialCommand(this, e, oldColor, oldIsFilled, oldIsStroked, oldStrokeWidth, oldAntiAliased, oldColor, oldIsFilled, oldIsStroked, oldStrokeWidth, newAntiAliased));
                  }
                  c->antiAliased = newAntiAliased;
                  m_canvas->update();
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
            [this, e, c](double val) {
              float oldEntryTime = c->entryTime;
              float oldExitTime = c->exitTime;
              float newEntryTime = val;
              if (oldEntryTime != newEntryTime) {
                m_undoStack->push(new ChangeAnimationCommand(this, e, oldEntryTime, oldExitTime, newEntryTime, oldExitTime));
              }
              c->entryTime = newEntryTime;
              m_canvas->update();
            });
        animationLayout->addRow(tr("Entry Time"), entryTimeSpin);

        auto *exitTimeSpin = new QDoubleSpinBox();
        exitTimeSpin->setRange(0.0, 1000.0);
        exitTimeSpin->setSingleStep(0.1);
        exitTimeSpin->setValue(c->exitTime);
        connect(
            exitTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this, e, c](double val) {
              float oldEntryTime = c->entryTime;
              float oldExitTime = c->exitTime;
              float newExitTime = val;
              if (oldExitTime != newExitTime) {
                m_undoStack->push(new ChangeAnimationCommand(this, e, oldEntryTime, oldExitTime, oldEntryTime, newExitTime));
              }
              c->exitTime = newExitTime;
              m_canvas->update();
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
                [this, e, c, scriptPathEdit](const QString &val) {
                  if (scriptPathEdit->property("textChangedByCode").toBool()) return;
                  std::string oldScriptPath = c->scriptPath;
                  std::string oldStartFunction = c->startFunction;
                  std::string oldUpdateFunction = c->updateFunction;
                  std::string oldDestroyFunction = c->destroyFunction;
                  std::string newScriptPath = val.toStdString();
                  if (oldScriptPath != newScriptPath) {
                    m_undoStack->push(new ChangeScriptCommand(this, e, oldScriptPath, oldStartFunction, oldUpdateFunction, oldDestroyFunction, newScriptPath, oldStartFunction, oldUpdateFunction, oldDestroyFunction));
                  }
                  c->scriptPath = newScriptPath;
                });
        connect(scriptPathEdit, &QLineEdit::textEdited, [scriptPathEdit](){
          scriptPathEdit->setProperty("textChangedByCode", false);
        });
        scriptPathEdit->setProperty("textChangedByCode", true);
        scriptLayout->addRow(tr("Path"), scriptPathEdit);

        auto *startFunctionEdit =
            new QLineEdit(scriptProps["startFunction"].toString());
        connect(startFunctionEdit, &QLineEdit::textChanged,
                [this, e, c, startFunctionEdit](const QString &val) {
                  if (startFunctionEdit->property("textChangedByCode").toBool()) return;
                  std::string oldScriptPath = c->scriptPath;
                  std::string oldStartFunction = c->startFunction;
                  std::string oldUpdateFunction = c->updateFunction;
                  std::string oldDestroyFunction = c->destroyFunction;
                  std::string newStartFunction = val.toStdString();
                  if (oldStartFunction != newStartFunction) {
                    m_undoStack->push(new ChangeScriptCommand(this, e, oldScriptPath, oldStartFunction, oldUpdateFunction, oldDestroyFunction, oldScriptPath, newStartFunction, oldUpdateFunction, oldDestroyFunction));
                  }
                  c->startFunction = newStartFunction;
                });
        connect(startFunctionEdit, &QLineEdit::textEdited, [startFunctionEdit](){
          startFunctionEdit->setProperty("textChangedByCode", false);
        });
        startFunctionEdit->setProperty("textChangedByCode", true);
        scriptLayout->addRow(tr("Start Function"), startFunctionEdit);

        auto *updateFunctionEdit =
            new QLineEdit(scriptProps["updateFunction"].toString());
        connect(updateFunctionEdit, &QLineEdit::textChanged,
                [this, e, c, updateFunctionEdit](const QString &val) {
                  if (updateFunctionEdit->property("textChangedByCode").toBool()) return;
                  std::string oldScriptPath = c->scriptPath;
                  std::string oldStartFunction = c->startFunction;
                  std::string oldUpdateFunction = c->updateFunction;
                  std::string oldDestroyFunction = c->destroyFunction;
                  std::string newUpdateFunction = val.toStdString();
                  if (oldUpdateFunction != newUpdateFunction) {
                    m_undoStack->push(new ChangeScriptCommand(this, e, oldScriptPath, oldStartFunction, oldUpdateFunction, oldDestroyFunction, oldScriptPath, oldStartFunction, newUpdateFunction, oldDestroyFunction));
                  }
                  c->updateFunction = newUpdateFunction;
                });
        connect(updateFunctionEdit, &QLineEdit::textEdited, [updateFunctionEdit](){
          updateFunctionEdit->setProperty("textChangedByCode", false);
        });
        updateFunctionEdit->setProperty("textChangedByCode", true);
        scriptLayout->addRow(tr("Update Function"), updateFunctionEdit);

        auto *destroyFunctionEdit =
            new QLineEdit(scriptProps["destroyFunction"].toString());
        connect(destroyFunctionEdit, &QLineEdit::textChanged,
                [this, e, c, destroyFunctionEdit](const QString &val) {
                  if (destroyFunctionEdit->property("textChangedByCode").toBool()) return;
                  std::string oldScriptPath = c->scriptPath;
                  std::string oldStartFunction = c->startFunction;
                  std::string oldUpdateFunction = c->updateFunction;
                  std::string oldDestroyFunction = c->destroyFunction;
                  std::string newDestroyFunction = val.toStdString();
                  if (oldDestroyFunction != newDestroyFunction) {
                    m_undoStack->push(new ChangeScriptCommand(this, e, oldScriptPath, oldStartFunction, oldUpdateFunction, oldDestroyFunction, oldScriptPath, oldStartFunction, oldUpdateFunction, newDestroyFunction));
                  }
                  c->destroyFunction = newDestroyFunction;
                });
        connect(destroyFunctionEdit, &QLineEdit::textEdited, [destroyFunctionEdit](){
          destroyFunctionEdit->setProperty("textChangedByCode", false);
        });
        destroyFunctionEdit->setProperty("textChangedByCode", true);
        scriptLayout->addRow(tr("Destroy Function"), destroyFunctionEdit);

        m_propsLayout->addRow(scriptGroup);
      }
    }
  }

private slots:
  void onNewFile() {
    m_canvas->scene().clear();
    m_canvas->scene().createBackground(m_canvas->width(), m_canvas->height());
    m_initialRegistry = m_canvas->scene().reg; // Save initial state
    m_sceneModel->refresh();
    m_canvas->update();
  }

  void onOpenFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Scene"), QString(), tr("Scene Files (*.json)"));
    if (filePath.isEmpty()) {
      return;
    }

    QFile loadFile(filePath);
    if (!loadFile.open(QIODevice::ReadOnly)) {
      qWarning("Couldn't open save file.");
      return;
    }

    QByteArray saveData = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

    m_canvas->scene().deserialize(loadDoc.object());
    m_initialRegistry = m_canvas->scene().reg; // Update initial state
    m_sceneModel->refresh();
    m_canvas->update();
  }

  void onSaveFile() {
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Scene"), QString(), tr("Scene Files (*.json)"));
    if (filePath.isEmpty()) {
      return;
    }

    QFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
      qWarning("Couldn't open save file.");
      return;
    }

    QJsonObject gameObject = m_canvas->scene().serialize();
    QJsonDocument saveDoc(gameObject);
    saveFile.write(saveDoc.toJson());
  }

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

  void onCut() {
    if (m_selectedEntities.isEmpty()) {
      return;
    }
    onCopy(); // Copy selected entities to clipboard
    m_undoStack->push(new DeleteCommand(this, m_selectedEntities)); // Then delete them
    m_selectedEntities.clear();
    m_canvas->setSelectedEntity(kInvalidEntity);
    m_sceneModel->refresh();
    m_canvas->update();
  }

  void onDelete() {
    if (m_selectedEntities.isEmpty()) {
      return;
    }
    m_undoStack->push(new DeleteCommand(this, m_selectedEntities));
    m_selectedEntities.clear();
    m_canvas->setSelectedEntity(kInvalidEntity);
    m_sceneModel->refresh();
    m_canvas->update();
  }

  void onCopy() {
    if (m_selectedEntities.isEmpty()) {
      return;
    }
    QJsonArray entitiesArray;
    for (Entity e : m_selectedEntities) {
      QJsonObject entityObject;
      // Serialize entity data
      if (auto *name = m_canvas->scene().reg.get<NameComponent>(e)) {
        entityObject["NameComponent"] = name->name.c_str();
      }
      if (auto *transform = m_canvas->scene().reg.get<TransformComponent>(e)) {
        QJsonObject transformObject;
        transformObject["x"] = transform->x;
        transformObject["y"] = transform->y;
        transformObject["rotation"] = transform->rotation;
        transformObject["sx"] = transform->sx;
        transformObject["sy"] = transform->sy;
        entityObject["TransformComponent"] = transformObject;
      }
      if (auto *material = m_canvas->scene().reg.get<MaterialComponent>(e)) {
        QJsonObject materialObject;
        materialObject["color"] = (qint64)material->color;
        materialObject["isFilled"] = material->isFilled;
        materialObject["isStroked"] = material->isStroked;
        materialObject["strokeWidth"] = material->strokeWidth;
        materialObject["antiAliased"] = material->antiAliased;
        entityObject["MaterialComponent"] = materialObject;
      }
      if (auto *animation = m_canvas->scene().reg.get<AnimationComponent>(e)) {
        QJsonObject animationObject;
        animationObject["entryTime"] = animation->entryTime;
        animationObject["exitTime"] = animation->exitTime;
        entityObject["AnimationComponent"] = animationObject;
      }
      if (auto *script = m_canvas->scene().reg.get<ScriptComponent>(e)) {
        QJsonObject scriptObject;
        scriptObject["scriptPath"] = script->scriptPath.c_str();
        scriptObject["startFunction"] = script->startFunction.c_str();
        scriptObject["updateFunction"] = script->updateFunction.c_str();
        scriptObject["destroyFunction"] = script->destroyFunction.c_str();
        entityObject["ScriptComponent"] = scriptObject;
      }
      if (m_canvas->scene().reg.has<SceneBackgroundComponent>(e)) {
        entityObject["SceneBackgroundComponent"] = true;
      }
      if (auto *shape = m_canvas->scene().reg.get<ShapeComponent>(e)) {
        QJsonObject shapeObject;
        shapeObject["kind"] = (int)shape->kind;
        shapeObject["properties"] = shape->getProperties().toJsonObject();
        entityObject["ShapeComponent"] = shapeObject;
      }
      entitiesArray.append(entityObject);
    }
    QJsonDocument doc(entitiesArray);
    QApplication::clipboard()->setText(doc.toJson(QJsonDocument::Compact));
  }

  void onPaste() {
    QString jsonString = QApplication::clipboard()->text();
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());

    if (doc.isArray()) {
      QJsonArray entitiesArray = doc.array();
      for (const QJsonValue &entityValue : entitiesArray) {
        if (entityValue.isObject()) {
          QJsonObject entityObject = entityValue.toObject();
          // Create a new entity and deserialize its components
          Entity newEntity = m_canvas->scene().reg.create();

          if (entityObject.contains("NameComponent") &&
              entityObject["NameComponent"].isString()) {
            QString baseName = entityObject["NameComponent"].toString();
            std::string uniqueName = baseName.toStdString();
            int counter = 1;
            bool nameExists = true;
            while (nameExists) {
              nameExists = false;
              m_canvas->scene().reg.each<NameComponent>([&](Entity ent, NameComponent &nameComp) {
                if (nameComp.name == uniqueName) {
                  nameExists = true;
                }
              });
              if (nameExists) {
                uniqueName = baseName.toStdString() + "." + std::to_string(counter++);
              }
            }
            m_canvas->scene().reg.emplace<NameComponent>(
                newEntity,
                NameComponent{uniqueName});
          }
          if (entityObject.contains("TransformComponent") &&
              entityObject["TransformComponent"].isObject()) {
            QJsonObject transformObject =
                entityObject["TransformComponent"].toObject();
            m_canvas->scene().reg.emplace<TransformComponent>(
                newEntity, TransformComponent{
                               (float)transformObject["x"].toDouble() +
                                   10, // Offset pasted entities
                               (float)transformObject["y"].toDouble() +
                                   10, // Offset pasted entities
                               (float)transformObject["rotation"].toDouble(),
                               (float)transformObject["sx"].toDouble(),
                               (float)transformObject["sy"].toDouble()});
          }
          if (entityObject.contains("MaterialComponent") &&
              entityObject["MaterialComponent"].isObject()) {
            QJsonObject materialObject =
                entityObject["MaterialComponent"].toObject();
            m_canvas->scene().reg.emplace<MaterialComponent>(
                newEntity,
                MaterialComponent{
                    (SkColor)materialObject["color"].toVariant().toULongLong(),
                    materialObject["isFilled"].toBool(),
                    materialObject["isStroked"].toBool(),
                    (float)materialObject["strokeWidth"].toDouble(),
                    materialObject["antiAliased"].toBool()});
          }
          if (entityObject.contains("AnimationComponent") &&
              entityObject["AnimationComponent"].isObject()) {
            QJsonObject animationObject =
                entityObject["AnimationComponent"].toObject();
            m_canvas->scene().reg.emplace<AnimationComponent>(
                newEntity, AnimationComponent{
                               (float)animationObject["entryTime"].toDouble(),
                               (float)animationObject["exitTime"].toDouble()});
          }
          if (entityObject.contains("ScriptComponent") &&
              entityObject["ScriptComponent"].isObject()) {
            QJsonObject scriptObject =
                entityObject["ScriptComponent"].toObject();
            m_canvas->scene().reg.emplace<ScriptComponent>(
                newEntity,
                ScriptComponent{
                    scriptObject["scriptPath"].toString().toStdString(),
                    scriptObject["startFunction"].toString().toStdString(),
                    scriptObject["updateFunction"].toString().toStdString(),
                    scriptObject["destroyFunction"].toString().toStdString()});
          }
          if (entityObject.contains("SceneBackgroundComponent") &&
              entityObject["SceneBackgroundComponent"].toBool()) {
            m_canvas->scene().reg.emplace<SceneBackgroundComponent>(newEntity);
          }
          if (entityObject.contains("ShapeComponent") &&
              entityObject["ShapeComponent"].isObject()) {
            QJsonObject shapeObject = entityObject["ShapeComponent"].toObject();
            ShapeComponent::Kind kind =
                (ShapeComponent::Kind)shapeObject["kind"].toInt();
            ShapeComponent shapeComp{kind, std::monostate{}};

            if (shapeObject.contains("properties") &&
                shapeObject["properties"].isObject()) {
              QJsonObject propsObject = shapeObject["properties"].toObject();
              switch (kind) {
              case ShapeComponent::Kind::Rectangle: {
                RectangleProperties props;
                const QMetaObject &metaObject = props.staticMetaObject;
                for (int i = metaObject.propertyOffset();
                     i < metaObject.propertyCount(); ++i) {
                  QMetaProperty metaProperty = metaObject.property(i);
                  if (propsObject.contains(metaProperty.name())) {
                    metaProperty.writeOnGadget(
                        &props, propsObject[metaProperty.name()].toVariant());
                  }
                }
                shapeComp.properties = props;
                break;
              }
              case ShapeComponent::Kind::Circle: {
                CircleProperties props;
                const QMetaObject &metaObject = props.staticMetaObject;
                for (int i = metaObject.propertyOffset();
                     i < metaObject.propertyCount(); ++i) {
                  QMetaProperty metaProperty = metaObject.property(i);
                  if (propsObject.contains(metaProperty.name())) {
                    metaProperty.writeOnGadget(
                        &props, propsObject[metaProperty.name()].toVariant());
                  }
                }
                shapeComp.properties = props;
                break;
              }
              case ShapeComponent::Kind::Line: {
                LineProperties props;
                const QMetaObject &metaObject = props.staticMetaObject;
                for (int i = metaObject.propertyOffset();
                     i < metaObject.propertyCount(); ++i) {
                  QMetaProperty metaProperty = metaObject.property(i);
                  if (propsObject.contains(metaProperty.name())) {
                    metaProperty.writeOnGadget(
                        &props, propsObject[metaProperty.name()].toVariant());
                  }
                }
                shapeComp.properties = props;
                break;
              }
              case ShapeComponent::Kind::Bezier: {
                BezierProperties props;
                const QMetaObject &metaObject = props.staticMetaObject;
                for (int i = metaObject.propertyOffset();
                     i < metaObject.propertyCount(); ++i) {
                  QMetaProperty metaProperty = metaObject.property(i);
                  if (propsObject.contains(metaProperty.name())) {
                    metaProperty.writeOnGadget(
                        &props, propsObject[metaProperty.name()].toVariant());
                  }
                }
                shapeComp.properties = props;
                break;
              }
              case ShapeComponent::Kind::Text: {
                TextProperties props;
                const QMetaObject &metaObject = props.staticMetaObject;
                for (int i = metaObject.propertyOffset();
                     i < metaObject.propertyCount(); ++i) {
                  QMetaProperty metaProperty = metaObject.property(i);
                  if (propsObject.contains(metaProperty.name())) {
                    metaProperty.writeOnGadget(
                        &props, propsObject[metaProperty.name()].toVariant());
                  }
                }
                shapeComp.properties = props;
                break;
              }
              case ShapeComponent::Kind::Image: {
                ImageProperties props;
                const QMetaObject &metaObject = props.staticMetaObject;
                for (int i = metaObject.propertyOffset();
                     i < metaObject.propertyCount(); ++i) {
                  QMetaProperty metaProperty = metaObject.property(i);
                  if (propsObject.contains(metaProperty.name())) {
                    metaProperty.writeOnGadget(
                        &props, propsObject[metaProperty.name()].toVariant());
                  }
                }
                shapeComp.properties = props;
                break;
              }
              }
            } else {
              // If properties are empty or missing, initialize with defaults
              // based on kind
              if (kind == ShapeComponent::Kind::Rectangle) {
                shapeComp.properties.emplace<RectangleProperties>();
              } else if (kind == ShapeComponent::Kind::Circle) {
                shapeComp.properties.emplace<CircleProperties>();
              } else if (kind == ShapeComponent::Kind::Line) {
                shapeComp.properties.emplace<LineProperties>();
              } else if (kind == ShapeComponent::Kind::Bezier) {
                shapeComp.properties.emplace<BezierProperties>();
              } else if (kind == ShapeComponent::Kind::Text) {
                shapeComp.properties.emplace<TextProperties>();
              } else if (kind == ShapeComponent::Kind::Image) {
                shapeComp.properties.emplace<ImageProperties>();
              }
            }
            m_canvas->scene().reg.emplace<ShapeComponent>(newEntity, shapeComp);
          }
          m_undoStack->push(new AddEntityCommand(this, newEntity));
        }
      }
    }
    m_sceneModel->refresh();
    m_canvas->update();
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

  void onTransformationCompleted(Entity entity, float oldX, float oldY, float oldRotation, float newX, float newY, float newRotation) {
    m_undoStack->push(new MoveEntityCommand(this, entity, oldX, oldY, oldRotation, newX, newY, newRotation));
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
  QUndoStack *m_undoStack = nullptr;
  QList<Entity>
      m_selectedEntities; // Track selected entities for cut/copy/paste
  bool m_isPlaying = false;
  float m_currentTime = 0.0f;
  float m_animationDuration = 5.0f; // Default animation duration
};
