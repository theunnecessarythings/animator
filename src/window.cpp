#include "window.h"
#include "serialization.h"

#include "commands.h"

#include <QAction>
#include <QComboBox>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMetaProperty>
#include <QMetaType>
#include <QProcess>
#include <QProgressDialog>
#include <QScrollArea>
#include <QtMath>
#include <type_traits>

Q_DECLARE_METATYPE(PathEffectComponent::Type);

template <typename NumWidget, typename Getter, typename Setter>
static NumWidget *makeSpinBox(QWidget *parent, double min, double max,
                              double step, Getter getter, Setter setter) {
  auto *w = new NumWidget(parent);
  w->setRange(min, max);

  if constexpr (std::is_same_v<NumWidget, QSpinBox>) {
    w->setValue(static_cast<int>(getter()));
    QObject::connect(w, QOverload<int>::of(&QSpinBox::valueChanged), parent,
                     [setter](int v) { setter(v); });
  } else {
    w->setSingleStep(step);
    w->setDecimals(3);
    w->setValue(getter());
    QObject::connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     parent, [setter](double v) { setter(v); });
  }
  return w;
}

inline QWidget *makeEditor(QWidget *parent, const QMetaProperty &mp,
                           std::function<void(QVariant)> setter,
                           const QVariant &initial) {
  switch (static_cast<int>(mp.type())) {
  case QMetaType::Float: {
    auto *w = new QDoubleSpinBox(parent);
    w->setRange(-1e6, 1e6);
    w->setDecimals(3);
    w->setValue(initial.toFloat());
    QObject::connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     parent,
                     [setter](double v) { setter(static_cast<float>(v)); });
    return w;
  }
  case QMetaType::Double: {
    auto *w = new QDoubleSpinBox(parent);
    w->setRange(-1e6, 1e6);
    w->setDecimals(3);
    w->setValue(initial.toDouble());
    QObject::connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     parent, [setter](double v) { setter(v); });
    return w;
  }
  case QMetaType::Int: {
    auto *w = new QSpinBox(parent);
    w->setRange(-1'000'000, 1'000'000);
    w->setValue(initial.toInt());
    QObject::connect(w, QOverload<int>::of(&QSpinBox::valueChanged), parent,
                     [setter](int v) { setter(v); });
    return w;
  }
  case QMetaType::QString: {
    auto *w = new QLineEdit(initial.toString(), parent);
    QObject::connect(w, &QLineEdit::textChanged, parent,
                     [setter](const QString &s) { setter(s); });
    return w;
  }
  case QMetaType::Bool: {
    auto *w = new QCheckBox(parent);
    w->setChecked(initial.toBool());
    QObject::connect(w, &QCheckBox::toggled, parent,
                     [setter](bool b) { setter(b); });
    return w;
  }
  case QMetaType::QColor: {
    auto *btn = new QPushButton(QObject::tr("Pick"), parent);

    auto updateBtnColor = [btn](const QColor &c) {
      QPalette pal(btn->palette());
      pal.setColor(QPalette::Button, c);
      btn->setPalette(pal);
      btn->setAutoFillBackground(true);
    };
    updateBtnColor(initial.value<QColor>());

    QObject::connect(btn, &QPushButton::clicked, parent, [=] {
      QColor c = QColorDialog::getColor(initial.value<QColor>(), parent);
      if (c.isValid()) {
        updateBtnColor(c);
        setter(c);
      }
    });
    return btn;
  }
  default:
    return new QLabel(QStringLiteral("<%1>").arg(mp.name()), parent);
  }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_canvas(new SkiaCanvasWidget(this)),
      m_undoStack(new QUndoStack(this)) {
  m_fileWatcher = new QFileSystemWatcher(this);
  connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
          &MainWindow::onScriptFileChanged);
  setCentralWidget(m_canvas);
  m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumSize(QSize(800, 700));
  createMenus();
  createToolbox();
  createSceneDock();
  createPropertiesDock();
  createTimelineDock();
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
  onNewFile();
}

MainWindow::~MainWindow() {
  disconnect(m_sceneTree->selectionModel(),
             &QItemSelectionModel::selectionChanged, this,
             &MainWindow::onSceneSelectionChanged);
  m_undoStack->clear();
  m_canvas->scene().clear();
}

void MainWindow::captureInitialScene() {
  m_initialSceneJson = m_canvas->scene().serialize();
}

void MainWindow::createMenus() {
  // --- File -----------------------------------------------------------
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(tr("&New"), this, &MainWindow::onNewFile);
  fileMenu->addAction(tr("&Open…"), this, &MainWindow::onOpenFile);
  fileMenu->addAction(tr("&Save"), this, &MainWindow::onSaveFile);
  fileMenu->addSeparator();
  fileMenu->addAction(tr("Render Video..."), this, &MainWindow::onRenderVideo);
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

  connect(m_undoStack, &QUndoStack::indexChanged, this, [this]() {
    if (m_sceneTree->selectionModel()->hasSelection()) {
      onSceneSelectionChanged(m_sceneTree->selectionModel()->selection(),
                              QItemSelection());
    }
  });
}

void MainWindow::createToolbox() {
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

void MainWindow::createSceneDock() {
  auto *sceneDock = new QDockWidget(tr("Scene"), this);
  sceneDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

  auto *tree = new QTreeView(sceneDock);

  m_sceneModel = new SceneModel(&m_canvas->scene(), tree);
  tree->setModel(m_sceneModel);
  tree->setHeaderHidden(true);

  m_sceneTree = tree;
  sceneDock->setWidget(tree);
  addDockWidget(Qt::RightDockWidgetArea, sceneDock);
  connect(m_canvas, &SkiaCanvasWidget::sceneChanged, m_sceneModel,
          &SceneModel::refresh);

  connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &MainWindow::onSceneSelectionChanged);

  connect(m_canvas, &SkiaCanvasWidget::transformChanged, this,
          &MainWindow::onTransformChanged);
  connect(m_canvas, &SkiaCanvasWidget::canvasSelectionChanged, this,
          &MainWindow::onCanvasSelectionChanged);
  connect(m_canvas, &SkiaCanvasWidget::transformationCompleted, this,
          &MainWindow::onTransformationCompleted);
  connect(m_canvas, &SkiaCanvasWidget::dragStarted, this,
          [this] { m_isDragging = true; });
  connect(m_canvas, &SkiaCanvasWidget::dragEnded, this,
          [this] { m_isDragging = false; });

  connect(m_canvas, &SkiaCanvasWidget::entityAdded, this, [this](Entity e) {
    m_undoStack->push(new AddEntityCommand(this, e));
  });
}

void MainWindow::createPropertiesDock() {
  auto *propDock = new QDockWidget(tr("Properties"), this);
  propDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

  auto *scrollArea = new QScrollArea(propDock);
  scrollArea->setWidgetResizable(true);

  auto *propWidget = new QWidget(scrollArea);
  m_propsLayout = new QFormLayout(propWidget);
  propWidget->setLayout(m_propsLayout);

  scrollArea->setWidget(propWidget);
  propDock->setWidget(scrollArea);
  propDock->setMinimumHeight(400);
  addDockWidget(Qt::RightDockWidgetArea, propDock);
}

void MainWindow::createTimelineDock() {
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
      0, static_cast<int>(m_animationDuration * 100)); // 100 units per
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

void MainWindow::onSceneSelectionChanged(const QItemSelection &sel,
                                         const QItemSelection &) {
  clearLayout(m_propsLayout);
  m_selectedEntities.clear();

  if (sel.indexes().isEmpty()) {
    m_canvas->setSelectedEntities({});
    m_canvas->update();
    return;
  }

  // Map tree indexes → entity list and sync canvas selection
  QList<Entity> selected;
  for (auto idx : sel.indexes())
    selected.append(m_sceneModel->getEntity(idx));
  m_canvas->setSelectedEntities(selected);
  m_canvas->update();

  if (selected.size() != 1)
    return; // multi‑select: no property panel
  Entity e = selected.first();

  // ============================= Name ===================================
  if (e.has<NameComponent>()) {
    auto &n = e.get<NameComponent>();
    auto *grp = new QGroupBox(tr("Object"));
    auto *form = new QFormLayout(grp);
    auto *edit = new QLineEdit(QString::fromStdString(n.name));
    form->addRow(tr("Name"), edit);
    connect(edit, &QLineEdit::textEdited, this, [this, e, edit] {
      if (e.has<NameComponent>()) {
        auto &nc = e.get_mut<NameComponent>();
        std::string oldName = nc.name;
        std::string newName = edit->text().toStdString();
        if (oldName != newName)
          m_undoStack->push(new ChangeNameCommand(this, e, oldName, newName));
      }
    });
    m_propsLayout->addRow(grp);
  }

  // ============================= Shape ==================================
  if (e.has<ShapeComponent>() && e.get<ShapeComponent>().shape) {
    auto &sc = e.get<ShapeComponent>();
    auto *grp = new QGroupBox(tr("Shape"));
    auto *lay = new QVBoxLayout(grp);
    lay->addWidget(new QLabel(sc.shape->getKindName()));
    QWidget *editor =
        sc.shape->createPropertyEditor(this, [this, e](QJsonObject props) {
          if (e.has<ShapeComponent>() && e.get<ShapeComponent>().shape) {
            auto &sc2 = e.get_mut<ShapeComponent>();
            QJsonObject oldProps = sc2.shape->serialize();
            m_undoStack->push(
                new ChangeShapePropertyCommand(this, e, oldProps, props));
            m_canvas->update();
          }
        });
    lay->addWidget(editor);
    m_propsLayout->addRow(grp);
  }

  // =========================== Transform ================================
  if (e.has<TransformComponent>()) {
    auto &tc = e.get<TransformComponent>();
    auto *grp = new QGroupBox(tr("Transform"));
    auto *form = new QFormLayout(grp);

    auto addSpin = [&](const QString &lbl, const char *objName, auto getter,
                       auto setter) {
      auto *sb =
          makeSpinBox<QDoubleSpinBox>(this, -10000, 10000, 0.1, getter, setter);
      sb->setObjectName(objName);
      form->addRow(lbl, sb);
    };
    addSpin(
        "X", "tx", [tc] { return tc.x; },
        [this, e](double v) {
          auto &tc = e.get_mut<TransformComponent>();
          auto oldX = tc.x;
          tc.x = v;
          m_undoStack->push(new ChangeTransformCommand(
              this, e, oldX, tc.y, tc.rotation, tc.sx, tc.sy, tc.x, tc.y,
              tc.rotation, tc.sx, tc.sy));
          m_canvas->update();
        });
    addSpin(
        "Y", "ty", [tc] { return tc.y; },
        [this, e](double v) {
          auto &tc = e.get_mut<TransformComponent>();
          auto oldY = tc.y;
          tc.y = v;
          m_undoStack->push(new ChangeTransformCommand(
              this, e, tc.x, oldY, tc.rotation, tc.sx, tc.sy, tc.x, tc.y,
              tc.rotation, tc.sx, tc.sy));
          m_canvas->update();
        });
    addSpin(
        "Rotation", "rot", [tc] { return tc.rotation * 180 / M_PI; },
        [this, e](double v) {
          auto &tc = e.get_mut<TransformComponent>();
          auto oldRotation = tc.rotation;
          tc.rotation = v * M_PI / 180;
          m_undoStack->push(new ChangeTransformCommand(
              this, e, tc.x, tc.y, oldRotation, tc.sx, tc.sy, tc.x, tc.y,
              tc.rotation, tc.sx, tc.sy));
          m_canvas->update();
        });
    addSpin(
        "Scale X", "sx", [tc] { return tc.sx; },
        [this, e](double v) {
          auto &tc = e.get_mut<TransformComponent>();
          auto oldSx = tc.sx;
          tc.sx = v;
          m_undoStack->push(new ChangeTransformCommand(
              this, e, tc.x, tc.y, tc.rotation, oldSx, tc.sy, tc.x, tc.y,
              tc.rotation, tc.sx, tc.sy));
        });
    addSpin(
        "Scale Y", "sy", [tc] { return tc.sy; },
        [this, e](double v) {
          auto &tc = e.get_mut<TransformComponent>();
          auto oldSy = tc.sy;
          tc.sy = v;
          m_undoStack->push(new ChangeTransformCommand(
              this, e, tc.x, tc.y, tc.rotation, tc.sx, oldSy, tc.x, tc.y,
              tc.rotation, tc.sx, tc.sy));
        });
    m_propsLayout->addRow(grp);
  }

  // ============================ Material ================================
  if (e.has<MaterialComponent>()) {
    auto &mc = e.get<MaterialComponent>();
    auto *grp = new QGroupBox(tr("Material"));
    auto *form = new QFormLayout(grp);

    // Color button -------------------------------------------------------
    auto *colorBtn = new QPushButton("Color");
    QPalette pal(colorBtn->palette());
    pal.setColor(QPalette::Button, QColor::fromRgba(mc.color));
    colorBtn->setPalette(pal);
    colorBtn->setAutoFillBackground(true);
    form->addRow("Color", colorBtn);
    connect(colorBtn, &QPushButton::clicked, this, [this, e, colorBtn] {
      if (e.has<MaterialComponent>()) {
        auto &mat = e.get_mut<MaterialComponent>();
        QColor chosen =
            QColorDialog::getColor(QColor::fromRgba(mat.color), this);
        if (chosen.isValid() && chosen.rgba() != mat.color) {
          auto old = mat;
          mat.color = chosen.rgba();
          m_undoStack->push(new ChangeMaterialCommand(
              this, e, old.color, old.isFilled, old.isStroked, old.strokeWidth,
              old.antiAliased, mat.color, mat.isFilled, mat.isStroked,
              mat.strokeWidth, mat.antiAliased));
          QPalette p(colorBtn->palette());
          p.setColor(QPalette::Button, chosen);
          colorBtn->setPalette(p);
          m_canvas->update();
        }
      }
    });

    // Helper for checkboxes ---------------------------------------------
    auto makeCheck = [&](const QString &lbl, bool initial, auto memberPtr) {
      auto *cb = new QCheckBox();
      cb->setChecked(initial);
      form->addRow(lbl, cb);
      connect(cb, &QCheckBox::toggled, this, [this, e, memberPtr, lbl](bool v) {
        if (e.has<MaterialComponent>()) {
          auto &mat = e.get_mut<MaterialComponent>();
          auto old = mat;
          mat.*memberPtr = v;
          m_undoStack->push(new ChangeMaterialCommand(
              this, e, old.color, old.isFilled, old.isStroked, old.strokeWidth,
              old.antiAliased, mat.color, mat.isFilled, mat.isStroked,
              mat.strokeWidth, mat.antiAliased));
          m_canvas->update();
        }
      });
    };
    makeCheck("Filled", mc.isFilled, &MaterialComponent::isFilled);
    makeCheck("Stroked", mc.isStroked, &MaterialComponent::isStroked);
    makeCheck("AA", mc.antiAliased, &MaterialComponent::antiAliased);

    // Stroke width spinbox ----------------------------------------------
    auto *swSB = makeSpinBox<QDoubleSpinBox>(
        this, 0, 100, 0.1, [mc] { return mc.strokeWidth; },
        [this, e](double v) {
          if (e.has<MaterialComponent>()) {
            auto &mat = e.get_mut<MaterialComponent>();
            auto old = mat;
            mat.strokeWidth = v;
            m_undoStack->push(new ChangeMaterialCommand(
                this, e, old.color, old.isFilled, old.isStroked,
                old.strokeWidth, old.antiAliased, mat.color, mat.isFilled,
                mat.isStroked, mat.strokeWidth, mat.antiAliased));
            m_canvas->update();
          }
        });
    form->addRow("Stroke W", swSB);
    m_propsLayout->addRow(grp);
  }

  buildAttachableComponentEditor<AnimationComponent>(
      e, tr("Animation"), [this, e](QFormLayout *form, AnimationComponent &ac) {
        auto *entrySB = makeSpinBox<QDoubleSpinBox>(
            this, 0, 1000, 0.1, [&ac] { return ac.entryTime; },
            [this, e](double v) {
              auto &anim = e.get_mut<AnimationComponent>();
              QJsonObject oldJson;
              oldJson["entryTime"] = anim.entryTime;
              oldJson["exitTime"] = anim.exitTime;
              anim.entryTime = v;
              QJsonObject newJson = oldJson;
              newJson["entryTime"] = anim.entryTime;
              newJson["exitTime"] = anim.exitTime;
              m_undoStack->push(new SetComponentCommand<AnimationComponent>(
                  this, e, oldJson, newJson));
              m_canvas->update();
            });
        auto *exitSB = makeSpinBox<QDoubleSpinBox>(
            this, 0, 1000, 0.1, [&ac] { return ac.exitTime; },
            [this, e](double v) {
              auto &anim = e.get_mut<AnimationComponent>();
              QJsonObject oldJson;
              oldJson["entryTime"] = anim.entryTime;
              oldJson["exitTime"] = anim.exitTime;
              anim.exitTime = v;
              QJsonObject newJson = oldJson;
              newJson["entryTime"] = anim.entryTime;
              newJson["exitTime"] = anim.exitTime;
              m_undoStack->push(new SetComponentCommand<AnimationComponent>(
                  this, e, oldJson, newJson));
              m_canvas->update();
            });
        form->addRow("Entry", entrySB);
        form->addRow("Exit", exitSB);
      });

  // ============================= Script ================================
  buildAttachableComponentEditor<ScriptComponent>(
      e, tr("Script"), [this, e](QFormLayout *form, ScriptComponent &sc) {
        // Path
        {
          auto *pathWidget = new QWidget();
          auto *pathLayout = new QHBoxLayout(pathWidget);
          pathLayout->setContentsMargins(0, 0, 0, 0);
          pathLayout->setSpacing(4);

          auto *pathEdit = new QLineEdit(QString::fromStdString(sc.scriptPath));
          pathLayout->addWidget(pathEdit);

          auto *browseBtn = new QPushButton("...");
          browseBtn->setFixedWidth(
              browseBtn->fontMetrics().horizontalAdvance("...") + 10);
          pathLayout->addWidget(browseBtn);

          form->addRow("Path", pathWidget);

          connect(
              pathEdit, &QLineEdit::editingFinished, this, [this, e, pathEdit] {
                if (e.has<ScriptComponent>()) {
                  auto &sc2 = e.get_mut<ScriptComponent>();
                  std::string oldValue = sc2.scriptPath;
                  std::string newValue = pathEdit->text().toStdString();

                  if (newValue == sc2.scriptPath)
                    return;

                  QJsonObject oldJson;
                  oldJson["scriptPath"] =
                      QString::fromStdString(sc2.scriptPath);
                  oldJson["startFunction"] =
                      QString::fromStdString(sc2.startFunction);
                  oldJson["updateFunction"] =
                      QString::fromStdString(sc2.updateFunction);
                  oldJson["drawFunction"] =
                      QString::fromStdString(sc2.drawFunction);
                  oldJson["destroyFunction"] =
                      QString::fromStdString(sc2.destroyFunction);

                  sc2.scriptPath = newValue;

                  QJsonObject newJson = oldJson;
                  newJson["scriptPath"] = QString::fromStdString(newValue);

                  m_undoStack->push(new SetComponentCommand<ScriptComponent>(
                      this, e, oldJson, newJson));

                  // Update file watcher
                  if (!oldValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(oldValue))
                                        .absoluteFilePath();
                    m_fileWatcher->removePath(abs_path);
                  }
                  if (!newValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(newValue))
                                        .absoluteFilePath();
                    m_fileWatcher->addPath(abs_path);
                  }
                }
              });

          connect(browseBtn, &QPushButton::clicked, this, [this, e, pathEdit] {
            QString filePath = QFileDialog::getOpenFileName(
                this, tr("Open Script"), "../scripts",
                tr("Lua Files (*.lua);;All Files (*)"));
            if (!filePath.isEmpty()) {
              QDir executableDir(QCoreApplication::applicationDirPath());
              QString relativePath = executableDir.relativeFilePath(filePath);
              pathEdit->setText(relativePath);
              if (e.has<ScriptComponent>()) {
                auto &sc2 = e.get_mut<ScriptComponent>();
                std::string oldValue = sc2.scriptPath;
                std::string newValue = relativePath.toStdString();
                if (newValue != sc2.scriptPath) {
                  QJsonObject oldJson;
                  oldJson["scriptPath"] =
                      QString::fromStdString(sc2.scriptPath);
                  oldJson["startFunction"] =
                      QString::fromStdString(sc2.startFunction);
                  oldJson["updateFunction"] =
                      QString::fromStdString(sc2.updateFunction);
                  oldJson["drawFunction"] =
                      QString::fromStdString(sc2.drawFunction);
                  oldJson["destroyFunction"] =
                      QString::fromStdString(sc2.destroyFunction);

                  sc2.scriptPath = newValue;

                  QJsonObject newJson = oldJson;
                  newJson["scriptPath"] = QString::fromStdString(newValue);

                  m_undoStack->push(new SetComponentCommand<ScriptComponent>(
                      this, e, oldJson, newJson));
                  // Update file watcher
                  qDebug() << "Adding file watcher for"
                           << QString::fromStdString(newValue);
                  if (!oldValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(oldValue))
                                        .absoluteFilePath();
                    m_fileWatcher->removePath(abs_path);
                  }
                  if (!newValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(newValue))
                                        .absoluteFilePath();
                    m_fileWatcher->addPath(abs_path);
                  }
                }
              }
            }
          });
        }

        // Other fields
        auto makeEdit = [&](const QString &lbl, const std::string &initial,
                            const QString &jsonKey) {
          auto *le = new QLineEdit(QString::fromStdString(initial));
          form->addRow(lbl, le);
          connect(
              le, &QLineEdit::editingFinished, this, [this, e, le, jsonKey] {
                if (e.has<ScriptComponent>()) {
                  auto &sc2 = e.get_mut<ScriptComponent>();
                  std::string newValue = le->text().toStdString();

                  QJsonObject oldJson;
                  oldJson["scriptPath"] =
                      QString::fromStdString(sc2.scriptPath);
                  oldJson["startFunction"] =
                      QString::fromStdString(sc2.startFunction);
                  oldJson["updateFunction"] =
                      QString::fromStdString(sc2.updateFunction);
                  oldJson["drawFunction"] =
                      QString::fromStdString(sc2.drawFunction);
                  oldJson["destroyFunction"] =
                      QString::fromStdString(sc2.destroyFunction);

                  if (newValue == oldJson[jsonKey].toString().toStdString())
                    return;

                  QJsonObject newJson = oldJson;
                  newJson[jsonKey] = QString::fromStdString(newValue);

                  m_undoStack->push(new SetComponentCommand<ScriptComponent>(
                      this, e, oldJson, newJson));
                }
              });
        };

        makeEdit("Start", sc.startFunction, "startFunction");
        makeEdit("Update", sc.updateFunction, "updateFunction");
        makeEdit("Draw", sc.drawFunction, "drawFunction");
        makeEdit("Destroy", sc.destroyFunction, "destroyFunction");
        auto *removeBtn =
            form->findChild<QPushButton *>(QStringLiteral("Remove Component"));
        if (removeBtn) {
          const std::string lastPath = sc.scriptPath;
          connect(removeBtn, &QPushButton::clicked, this, [this, lastPath] {
            if (!lastPath.empty())
              m_fileWatcher->removePath(QString::fromStdString(lastPath));
          });
        }
      });

  // ======================= CppScriptComponent ========================
  buildAttachableComponentEditor<CppScriptComponent>(
      e, tr("C++ Script"),
      [this, e](QFormLayout *form, CppScriptComponent &csc) {
        // Path
        {
          auto *pathWidget = new QWidget();
          auto *pathLayout = new QHBoxLayout(pathWidget);
          pathLayout->setContentsMargins(0, 0, 0, 0);
          pathLayout->setSpacing(4);

          auto *pathEdit =
              new QLineEdit(QString::fromStdString(csc.source_path));
          pathLayout->addWidget(pathEdit);

          auto *browseBtn = new QPushButton("...");
          browseBtn->setFixedWidth(
              browseBtn->fontMetrics().horizontalAdvance("...") + 10);
          pathLayout->addWidget(browseBtn);

          form->addRow("Source Path", pathWidget);

          connect(
              pathEdit, &QLineEdit::editingFinished, this, [this, e, pathEdit] {
                if (e.has<CppScriptComponent>()) {
                  auto &csc2 = e.get_mut<CppScriptComponent>();
                  std::string oldValue = csc2.source_path;
                  std::string newValue = pathEdit->text().toStdString();

                  if (newValue == csc2.source_path)
                    return;

                  QJsonObject oldJson;
                  oldJson["source_path"] =
                      QString::fromStdString(csc2.source_path);

                  csc2.source_path = newValue;

                  QJsonObject newJson = oldJson;
                  newJson["source_path"] = QString::fromStdString(newValue);

                  m_undoStack->push(new SetComponentCommand<CppScriptComponent>(
                      this, e, oldJson, newJson));

                  // Update file watcher
                  if (!oldValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(oldValue))
                                        .absoluteFilePath();
                    m_fileWatcher->removePath(abs_path);
                  }
                  if (!newValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(newValue))
                                        .absoluteFilePath();
                    m_fileWatcher->addPath(abs_path);
                  }
                }
              });

          connect(browseBtn, &QPushButton::clicked, this, [this, e, pathEdit] {
            QString filePath = QFileDialog::getOpenFileName(
                this, tr("Open C++ Script"), "../scripts",
                tr("C++ Files (*.cpp);;All Files (*)"));
            if (!filePath.isEmpty()) {
              QDir executableDir(QCoreApplication::applicationDirPath());
              QString relativePath = executableDir.relativeFilePath(filePath);
              pathEdit->setText(relativePath);
              if (e.has<CppScriptComponent>()) {
                auto &csc2 = e.get_mut<CppScriptComponent>();
                std::string oldValue = csc2.source_path;
                std::string newValue = relativePath.toStdString();
                if (newValue != csc2.source_path) {
                  QJsonObject oldJson;
                  oldJson["source_path"] =
                      QString::fromStdString(csc2.source_path);
                  csc2.source_path = newValue;

                  QJsonObject newJson = oldJson;
                  newJson["source_path"] = QString::fromStdString(newValue);

                  m_undoStack->push(new SetComponentCommand<CppScriptComponent>(
                      this, e, oldJson, newJson));
                  // Update file watcher
                  qDebug() << "Adding file watcher for"
                           << QString::fromStdString(newValue);
                  if (!oldValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(oldValue))
                                        .absoluteFilePath();
                    m_fileWatcher->removePath(abs_path);
                  }
                  if (!newValue.empty()) {
                    auto abs_path = QFileInfo(QString::fromStdString(newValue))
                                        .absoluteFilePath();
                    m_fileWatcher->addPath(abs_path);
                  }
                }
              }
            }
          });
        }
      });

  // ========================== Path Effect ===============================
  buildAttachableComponentEditor<PathEffectComponent>(
      e, tr("Path Effect"),
      [this, e](QFormLayout *form, PathEffectComponent &pec) {
        // Type dropdown
        auto *typeCombo = new QComboBox();
        typeCombo->addItem(
            "None", QVariant::fromValue(PathEffectComponent::Type::None));
        typeCombo->addItem(
            "Dash", QVariant::fromValue(PathEffectComponent::Type::Dash));
        typeCombo->addItem(
            "Corner", QVariant::fromValue(PathEffectComponent::Type::Corner));
        typeCombo->addItem(
            "Discrete",
            QVariant::fromValue(PathEffectComponent::Type::Discrete));

        int initialIndex = typeCombo->findData(QVariant::fromValue(pec.type));
        typeCombo->setCurrentIndex(initialIndex);
        form->addRow("Type", typeCombo);

        // --- Create parameter editor groups ---
        auto *dashGroup = new QWidget();
        auto *dashForm = new QFormLayout(dashGroup);
        dashForm->setContentsMargins(0, 0, 0, 0);
        auto *cornerGroup = new QWidget();
        auto *cornerForm = new QFormLayout(cornerGroup);
        cornerForm->setContentsMargins(0, 0, 0, 0);
        auto *discreteGroup = new QWidget();
        auto *discreteForm = new QFormLayout(discreteGroup);
        discreteForm->setContentsMargins(0, 0, 0, 0);

        // --- Dash Editor ---
        {
          QString intervals;
          for (size_t i = 0; i < pec.dashIntervals.size(); ++i) {
            intervals += QString::number(pec.dashIntervals[i]);
            if (i < pec.dashIntervals.size() - 1)
              intervals += ", ";
          }
          auto *intervalsEdit = new QLineEdit(intervals);
          dashForm->addRow("Intervals", intervalsEdit);
          connect(intervalsEdit, &QLineEdit::editingFinished, this,
                  [this, e, intervalsEdit] {
                    if (e.has<PathEffectComponent>()) {
                      auto &pec2 = e.get_mut<PathEffectComponent>();
                      QStringList parts =
                          intervalsEdit->text().split(',', Qt::SkipEmptyParts);
                      std::vector<float> newIntervals;
                      bool ok = true;
                      for (const QString &part : parts) {
                        newIntervals.push_back(part.toFloat(&ok));
                        if (!ok)
                          break;
                      }
                      if (ok) {
                        pec2.dashIntervals = newIntervals;
                        m_canvas->update();
                      }
                    }
                  });

          auto *phaseSpin = makeSpinBox<QDoubleSpinBox>(
              this, 0, 1000, 0.1, [&pec] { return pec.dashPhase; },
              [this, e](double v) {
                if (e.has<PathEffectComponent>()) {
                  e.get_mut<PathEffectComponent>().dashPhase = v;
                  m_canvas->update();
                }
              });
          dashForm->addRow("Phase", phaseSpin);
        }

        // --- Corner Editor ---
        {
          auto *radiusSpin = makeSpinBox<QDoubleSpinBox>(
              this, 0, 1000, 0.1, [&pec] { return pec.cornerRadius; },
              [this, e](double v) {
                if (e.has<PathEffectComponent>()) {
                  e.get_mut<PathEffectComponent>().cornerRadius = v;
                  m_canvas->update();
                }
              });
          cornerForm->addRow("Radius", radiusSpin);
        }

        // --- Discrete Editor ---
        {
          auto *lengthSpin = makeSpinBox<QDoubleSpinBox>(
              this, 0, 1000, 0.1, [&pec] { return pec.discreteLength; },
              [this, e](double v) {
                if (e.has<PathEffectComponent>()) {
                  e.get_mut<PathEffectComponent>().discreteLength = v;
                  m_canvas->update();
                }
              });
          discreteForm->addRow("Length", lengthSpin);

          auto *devSpin = makeSpinBox<QDoubleSpinBox>(
              this, 0, 1000, 0.1, [&pec] { return pec.discreteDeviation; },
              [this, e](double v) {
                if (e.has<PathEffectComponent>()) {
                  e.get_mut<PathEffectComponent>().discreteDeviation = v;
                  m_canvas->update();
                }
              });
          discreteForm->addRow("Deviation", devSpin);
        }

        // Add groups to the main form layout
        form->addRow(dashGroup);
        form->addRow(cornerGroup);
        form->addRow(discreteGroup);

        // Visibility & update logic
        auto updateVisibility = [=](int index) {
          auto type =
              typeCombo->itemData(index).value<PathEffectComponent::Type>();
          dashGroup->setVisible(type == PathEffectComponent::Type::Dash);
          cornerGroup->setVisible(type == PathEffectComponent::Type::Corner);
          discreteGroup->setVisible(type ==
                                    PathEffectComponent::Type::Discrete);
        };

        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, e, typeCombo, updateVisibility](int index) {
                  if (e.has<PathEffectComponent>()) {
                    e.get_mut<PathEffectComponent>().type =
                        typeCombo->itemData(index)
                            .value<PathEffectComponent::Type>();
                    updateVisibility(index);
                    m_canvas->update();
                  }
                });

        updateVisibility(typeCombo->currentIndex());
      });

  // Add Component button
  auto *addComponentBtn = new QPushButton(tr("Add Component"));
  auto *addComponentMenu = new QMenu(addComponentBtn);
  addComponentBtn->setMenu(addComponentMenu);

  if (!e.has<AnimationComponent>()) {
    QAction *addAnimationAction = addComponentMenu->addAction(tr("Animation"));
    connect(addAnimationAction, &QAction::triggered, this, [this, e] {
      AnimationComponent defaultInstance;
      QJsonObject newJson;
      newJson["entryTime"] = defaultInstance.entryTime;
      newJson["exitTime"] = defaultInstance.exitTime;
      m_undoStack->push(new SetComponentCommand<AnimationComponent>(
          this, e, QJsonObject(), newJson));
    });
  }

  if (!e.has<ScriptComponent>()) {
    QAction *addScriptAction = addComponentMenu->addAction(tr("Script"));
    connect(addScriptAction, &QAction::triggered, this, [this, e] {
      ScriptComponent defaultInstance;
      QJsonObject newJson;
      newJson["scriptPath"] =
          QString::fromStdString(defaultInstance.scriptPath);
      newJson["startFunction"] =
          QString::fromStdString(defaultInstance.startFunction);
      newJson["updateFunction"] =
          QString::fromStdString(defaultInstance.updateFunction);
      newJson["drawFunction"] =
          QString::fromStdString(defaultInstance.drawFunction);
      newJson["destroyFunction"] =
          QString::fromStdString(defaultInstance.destroyFunction);
      m_undoStack->push(new SetComponentCommand<ScriptComponent>(
          this, e, QJsonObject(), newJson));
    });
  }

  if (!e.has<CppScriptComponent>()) {
    QAction *addCppScriptAction = addComponentMenu->addAction(tr("C++ Script"));
    connect(addCppScriptAction, &QAction::triggered, this, [this, e] {
      CppScriptComponent defaultInstance;
      QJsonObject newJson;
      newJson["source_path"] =
          QString::fromStdString(defaultInstance.source_path);
      m_undoStack->push(new SetComponentCommand<CppScriptComponent>(
          this, e, QJsonObject(), newJson));
      // Manually refresh the properties panel to show the new component
      onSceneSelectionChanged(m_sceneTree->selectionModel()->selection(),
                              QItemSelection());
    });
  }

  if (!e.has<PathEffectComponent>()) {
    QAction *addPathEffectAction =
        addComponentMenu->addAction(tr("Path Effect"));
    connect(addPathEffectAction, &QAction::triggered, this, [this, e] {
      PathEffectComponent defaultInstance;
      QJsonObject newJson;
      newJson["type"] = static_cast<int>(defaultInstance.type);
      QJsonArray dashIntervalsArray;
      for (float val : defaultInstance.dashIntervals) {
        dashIntervalsArray.append(val);
      }
      newJson["dashIntervals"] = dashIntervalsArray;
      newJson["dashPhase"] = defaultInstance.dashPhase;
      newJson["cornerRadius"] = defaultInstance.cornerRadius;
      newJson["discreteLength"] = defaultInstance.discreteLength;
      newJson["discreteDeviation"] = defaultInstance.discreteDeviation;
      m_undoStack->push(new SetComponentCommand<PathEffectComponent>(
          this, e, QJsonObject(), newJson));
    });
  }

  if (addComponentMenu->actions().isEmpty()) {
    addComponentBtn->hide();
  }

  m_propsLayout->addRow(addComponentBtn);
}

template <typename T>
void MainWindow::buildAttachableComponentEditor(
    Entity e, const QString &groupName,
    std::function<void(QFormLayout *, T &)> editorBuilder) {
  if (e.has<T>()) {
    auto *grp = new QGroupBox(groupName);
    auto *form = new QFormLayout(grp);

    editorBuilder(form, e.get_mut<T>());

    auto *removeBtn = new QPushButton(tr("Remove Component"));
    connect(removeBtn, &QPushButton::clicked, this, [this, e] {
      QJsonObject oldJson = serializeEntity(m_canvas->scene(), e)
                                .value(SceneCommand::getComponentJsonKey<T>())
                                .toObject();
      m_undoStack->push(
          new SetComponentCommand<T>(this, e, oldJson, QJsonObject()));
    });
    form->addRow(removeBtn);

    m_propsLayout->addRow(grp);
  }
}

void MainWindow::onNewFile() {
  m_canvas->setSceneResetting(true);
  m_undoStack->clear();
  m_fileWatcher->removePaths(m_fileWatcher->files());
  m_canvas->setSelectedEntities({});
  m_canvas->resetSceneAndDeserialize({});
  m_sceneModel->setScene(&m_canvas->scene());
  m_sceneModel->refresh();
  m_canvas->scene().createBackground(m_canvas->width(), m_canvas->height());

  m_canvas->update();
  m_canvas->setSceneResetting(false);
}

void MainWindow::onOpenFile() {
  m_canvas->setSceneResetting(true);
  m_undoStack->clear();
  m_fileWatcher->removePaths(m_fileWatcher->files());
  QString filePath = QFileDialog::getOpenFileName(this, tr("Open Scene"), {},
                                                  tr("Scene Files(*.json)"));
  if (filePath.isEmpty()) {
    m_canvas->setSceneResetting(false);
    return;
  }

  QFile loadFile(filePath);
  if (!loadFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open scene file.");
    m_canvas->setSceneResetting(false);
    return;
  }

  QJsonDocument doc(QJsonDocument::fromJson(loadFile.readAll()));
  m_canvas->setSelectedEntities({});
  m_canvas->resetSceneAndDeserialize(doc.object());
  m_sceneModel->setScene(&m_canvas->scene());
  m_sceneModel->refresh();
  m_canvas->update();
  m_canvas->setSceneResetting(false);
}

void MainWindow::onSaveFile() {
  QString filePath = QFileDialog::getSaveFileName(this, tr("Save Scene"), {},
                                                  tr("Scene Files(*.json)"));
  if (filePath.isEmpty())
    return;

  QFile saveFile(filePath);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open scene file for writing.");
    return;
  }

  QJsonDocument saveDoc(m_canvas->scene().serialize());
  saveFile.write(saveDoc.toJson());
}

void MainWindow::resetScene() {
  m_canvas->scene().clear();
  m_canvas->scene().deserialize(m_initialSceneJson);

  m_sceneModel->refresh();
  if (!m_sceneTree->selectionModel()->selectedIndexes().isEmpty())
    onSceneSelectionChanged(m_sceneTree->selectionModel()->selection(),
                            QItemSelection());
}

void MainWindow::onRenderVideo() {
  // Get resolution from user
  bool ok;
  QStringList items;
  items << QString("Current (%1x%2)")
               .arg(m_canvas->width())
               .arg(m_canvas->height())
        << "HD (1280x720)"
        << "Full HD (1920x1080)"
        << "4K (3840x2160)";
  QString item = QInputDialog::getItem(this, tr("Select Render Resolution"),
                                       tr("Resolution:"), items, 0, false, &ok);
  if (!ok || item.isEmpty()) {
    return; // User cancelled
  }

  int targetWidth, targetHeight;
  if (item.startsWith("Current")) {
    targetWidth = m_canvas->width();
    targetHeight = m_canvas->height();
  } else if (item.startsWith("HD")) {
    targetWidth = 1280;
    targetHeight = 720;
  } else if (item.startsWith("Full HD")) {
    targetWidth = 1920;
    targetHeight = 1080;
  } else if (item.startsWith("4K")) {
    targetWidth = 3840;
    targetHeight = 2160;
  } else {
    return; // Should not happen
  }

  QString videoPath = QFileDialog::getSaveFileName(this, tr("Render Video"), "",
                                                   tr("MP4 Video (*.mp4)"));
  if (videoPath.isEmpty()) {
    return;
  }

  // Ensure the filename has the .mp4 extension
  if (!videoPath.endsWith(".mp4", Qt::CaseInsensitive)) {
    videoPath += ".mp4";
  }

  // Settings
  const int fps = 60;
  const float duration = m_animationDuration;
  const int totalFrames = static_cast<int>(duration * fps);

  if (totalFrames <= 0) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Animation duration is zero. Nothing to render."));
    return;
  }

  // Save current state and reset for rendering
  QJsonObject sceneState = m_canvas->scene().serialize();
  auto selectionState = m_selectedEntities;

  // Prepare for rendering
  m_animationTimer->stop();
  m_isPlaying = false;
  m_playPauseButton->setText("Play");
  m_currentTime = 0.f;
  m_canvas->scene().getScriptSystem().resetEnvironments();
  m_canvas->setCurrentTime(m_currentTime);
  m_timelineSlider->setValue(0);
  updateTimeDisplay();

  QProgressDialog progress("Rendering video...", "Cancel", 0, totalFrames,
                           this);
  progress.setWindowModality(Qt::WindowModal);

  // Setup ffmpeg
  QString ffmpegPath = "ffmpeg"; // Assume ffmpeg is in PATH
  QStringList args;
  args << "-y"
       << "-f"
       << "rawvideo"
       << "-pix_fmt"
       << "rgba"
       << "-s" << QString("%1x%2").arg(targetWidth).arg(targetHeight) << "-r"
       << QString::number(fps) << "-i"
       << "-" // Input from stdin
       << "-vf"
       << "crop=trunc(iw/2)*2:trunc(ih/2)*2"
       << "-c:v"
       << "libx264"
       << "-pix_fmt"
       << "yuv420p" << videoPath;

  QProcess ffmpegProcess;
  ffmpegProcess.start(ffmpegPath, args);
  if (!ffmpegProcess.waitForStarted()) {
    QMessageBox::critical(
        this, "Error",
        "Could not start ffmpeg. Is it installed and in your PATH?");
    m_canvas->resetSceneAndDeserialize(sceneState); // Restore
    return;
  }

  m_canvas->setVideoRendering(true);

  for (int i = 0; i < totalFrames; ++i) {
    progress.setValue(i);
    if (progress.wasCanceled()) {
      break;
    }
    qApp->processEvents();

    float currentTime = static_cast<float>(i) / fps;

    // Update scene for the frame
    m_canvas->scene().getScriptSystem().tick(1.0f / fps, currentTime);

    // Render the high-resolution frame offscreen
    QImage frame =
        m_canvas->renderHighResFrame(targetWidth, targetHeight, currentTime);

    if (frame.isNull()) {
      qWarning() << "Failed to render high-res frame" << i;
      continue;
    }

    ffmpegProcess.write(reinterpret_cast<const char *>(frame.constBits()),
                        frame.sizeInBytes());
  }

  m_canvas->setVideoRendering(false);
  progress.setValue(totalFrames);

  ffmpegProcess.closeWriteChannel();
  if (!ffmpegProcess.waitForFinished(30000)) { // 30s timeout
    ffmpegProcess.kill();
    QMessageBox::critical(this, "Error",
                          "ffmpeg timed out or failed to finish.");
    qWarning() << "ffmpeg stderr:" << ffmpegProcess.readAllStandardError();
  } else {
    QMessageBox::information(this, "Success", "Video rendering complete.");
  }

  // Restore original scene state
  m_canvas->resetSceneAndDeserialize(sceneState);
  m_canvas->setSelectedEntities(selectionState);
  m_sceneModel->refresh();
  m_canvas->update();
}

void MainWindow::onCut() {
  if (m_selectedEntities.isEmpty())
    return;
  onCopy();
  m_undoStack->push(new DeleteCommand(this, m_selectedEntities));
  m_selectedEntities.clear();
  m_canvas->setSelectedEntities({});
  m_sceneModel->refresh();
  m_canvas->update();
}

void MainWindow::onDelete() {
  if (m_selectedEntities.isEmpty())
    return;
  m_undoStack->push(new DeleteCommand(this, m_selectedEntities));
  m_selectedEntities.clear();
  m_canvas->setSelectedEntities({});
  m_sceneModel->refresh();
  m_canvas->update();
}

void MainWindow::onCopy() {
  if (m_selectedEntities.isEmpty())
    return;

  QJsonArray arr;
  for (Entity e : std::as_const(m_selectedEntities))
    arr.append(serializeEntity(m_canvas->scene(), e));

  QApplication::clipboard()->setText(
      QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void MainWindow::onPaste() {
  const QString clip = QApplication::clipboard()->text();
  const QJsonDocument doc = QJsonDocument::fromJson(clip.toUtf8());
  if (!doc.isArray())
    return;

  auto &world = m_canvas->scene().ecs();
  m_undoStack->beginMacro("Paste");

  for (const QJsonValue &v : doc.array()) {
    if (!v.isObject())
      continue;

    Entity e = world.entity();
    applyJsonToEntity(world, e, v.toObject(), true);
    m_undoStack->push(new AddEntityCommand(this, e));
  }
  m_undoStack->endMacro();

  m_sceneModel->refresh();
  m_canvas->update();
}

void MainWindow::onTransformChanged(Entity entity) {
  if (m_isUpdatingFromUI) {
    return;
  }

  if (m_isDragging) {
    syncTransformEditors(entity);
    return;
  }

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

void MainWindow::onCanvasSelectionChanged(const QList<Entity> &entities) {
  m_selectedEntities = entities;
  QItemSelection selection;
  for (Entity entity : entities) {
    QModelIndex index = m_sceneModel->indexOfEntity(entity);
    if (index.isValid()) {
      selection.select(index, index);
    }
  }
  m_sceneTree->selectionModel()->select(selection,
                                        QItemSelectionModel::ClearAndSelect);
}

void MainWindow::onTransformationCompleted(Entity entity, float oldX,
                                           float oldY, float oldRotation,
                                           float newX, float newY,
                                           float newRotation) {
  m_isDragging = false;
  m_undoStack->push(new MoveEntityCommand(this, entity, oldX, oldY, oldRotation,
                                          newX, newY, newRotation));
}

void MainWindow::clearLayout(QLayout *layout) {
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

void MainWindow::updateTimeDisplay() {
  m_timeDisplayLabel->setText(QString("%1s / %2s")
                                  .arg(m_currentTime, 0, 'f', 2)
                                  .arg(m_animationDuration, 0, 'f', 2));
  m_canvas->update();
}

void MainWindow::onStopResetButtonClicked() {
  m_animationTimer->stop();
  m_isPlaying = false;
  m_playPauseButton->setText("Play");
  m_currentTime = 0.f;
  m_canvas->scene().getScriptSystem().resetEnvironments();
  if (!m_preSimulationState.isEmpty()) {
    m_canvas->setCurrentTime(m_currentTime);
    m_canvas->setSelectedEntities({});
    onSceneSelectionChanged({}, {});
    m_canvas->resetSceneAndDeserialize(m_preSimulationState);
  }

  m_sceneModel->refresh();
  m_canvas->update();
  m_timelineSlider->setValue(0);
  updateTimeDisplay();
}

void MainWindow::onAnimationTimerTimeout() {
  m_currentTime += m_animationTimer->interval() / 1000.f;
  if (m_currentTime > m_animationDuration) {
    m_canvas->scene().getScriptSystem().resetEnvironments();
    m_currentTime = 0.f;
    if (!m_preSimulationState.isEmpty()) {
      m_animationTimer->stop();
      m_isPlaying = false;
      m_playPauseButton->setText("Play");
      m_canvas->setCurrentTime(m_currentTime);
      m_canvas->setSelectedEntities({});
      onSceneSelectionChanged({}, {});
      m_canvas->resetSceneAndDeserialize(m_preSimulationState);
      m_sceneModel->refresh();
      m_canvas->update();
      m_timelineSlider->setValue(0);
      updateTimeDisplay();
      return;
    }
  }

  m_canvas->setCurrentTime(m_currentTime);
  m_canvas->scene().getScriptSystem().tick(
      m_animationTimer->interval() / 1000.f, m_currentTime);

  m_canvas->scene().update(m_animationTimer->interval() / 1000.f,
                           m_currentTime);

  m_canvas->update();

  m_timelineSlider->setValue(static_cast<int>(m_currentTime * 100));
  updateTimeDisplay();
}

void MainWindow::onTimelineSliderMoved(int value) {
  m_currentTime = value / 100.f;
  m_canvas->setCurrentTime(m_currentTime);
  m_canvas->update();
  updateTimeDisplay();
}

void MainWindow::onPlayPauseButtonClicked() {
  if (m_isPlaying) {
    // Pause
    m_animationTimer->stop();
    m_playPauseButton->setText("Play");
  } else {
    // Play
    // if (m_currentTime == 0.f) {
    //   m_preSimulationState = m_canvas->scene().serialize();
    // }
    m_animationTimer->start();
    m_playPauseButton->setText("Pause");
  }
  m_isPlaying = !m_isPlaying;
}

void MainWindow::syncTransformEditors(Entity e) {
  if (e.has<TransformComponent>()) {
    auto &tr = e.get<TransformComponent>();
    auto upd = [&](const char *name, double v) {
      if (auto *sb = findChild<QDoubleSpinBox *>(name)) {
        QSignalBlocker blk(sb); // avoid feedback
        sb->setValue(v);
      }
    };

    upd("tx", tr.x);
    upd("ty", tr.y);
    upd("rot", tr.rotation * 180.0 / M_PI);
    upd("sx", tr.sx);
    upd("sy", tr.sy);
  }
}

void MainWindow::onScriptFileChanged(const QString &path) {
  qDebug() << "Script file changed:" << path;

  if (path.endsWith(".lua")) {
    auto &ss = m_canvas->scene().getScriptSystem();
    ss.reloadScript(path.toStdString());
  } else if (path.endsWith(".cpp")) {
    auto &scene = m_canvas->scene();
    flecs::entity e = scene.findEntityByCppScriptPath(path.toStdString());
    if (e.is_valid()) {
      // Re-set the component to trigger the OnSet observer, which will
      // recompile and reload the script.
      e.set<CppScriptComponent>({path.toStdString()});
    }
  }

  onStopResetButtonClicked();
  onPlayPauseButtonClicked();
}
