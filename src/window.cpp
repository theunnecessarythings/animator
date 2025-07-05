#include "window.h"
#include "serialization.h"

#include <QAction>
#include <QMenu>
#include <QMetaProperty>
#include <QMetaType>
#include <QObject>
#include <QtMath>
#include <type_traits>

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
  setCentralWidget(m_canvas);

  createMenus();
  createToolbox();
  createSceneDock();
  createPropertiesDock();
  createTimelineDock();

  connect(m_canvas, &SkiaCanvasWidget::sceneChanged, this,
          [this] { m_sceneModel->refresh(); });

  captureInitialScene();
}

MainWindow::~MainWindow() {
  disconnect(m_sceneTree->selectionModel(),
             &QItemSelectionModel::selectionChanged, this,
             &MainWindow::onSceneSelectionChanged);
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
  sceneDock->setMaximumHeight(200);
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
}

void MainWindow::createPropertiesDock() {
  auto *propDock = new QDockWidget(tr("Properties"), this);
  propDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

  auto *propWidget = new QWidget(propDock);
  m_propsLayout = new QFormLayout(propWidget);
  propWidget->setLayout(m_propsLayout);

  propDock->setWidget(propWidget);
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

void MainWindow::onPlayPauseButtonClicked() {
  if (m_isPlaying) {
    m_animationTimer->stop();
    m_playPauseButton->setText("Play");
  } else {
    m_animationTimer->start();
    m_playPauseButton->setText("Pause");
  }
  m_isPlaying = !m_isPlaying;
}

void MainWindow::onStopResetButtonClicked() {
  m_animationTimer->stop();
  m_isPlaying = false;
  m_playPauseButton->setText("Play");
  m_currentTime = 0.f;

  resetScene();

  m_canvas->setCurrentTime(m_currentTime);
  m_canvas->update();
  m_timelineSlider->setValue(0);
  updateTimeDisplay();
}

void MainWindow::onAnimationTimerTimeout() {
  m_currentTime += m_animationTimer->interval() / 1000.f;
  if (m_currentTime > m_animationDuration) {
    resetScene();
    m_currentTime = 0.f;
  }

  m_canvas->setCurrentTime(m_currentTime);
  m_canvas->update();
  m_canvas->scene().scriptSystem.tick(m_animationTimer->interval() / 1000.f,
                                      m_currentTime);
  m_timelineSlider->setValue(static_cast<int>(m_currentTime * 100));
  updateTimeDisplay();
}

void MainWindow::onTimelineSliderMoved(int value) {
  m_currentTime = value / 100.f;
  m_canvas->setCurrentTime(m_currentTime);
  m_canvas->update();
  updateTimeDisplay();
}

void MainWindow::updateTimeDisplay() {
  m_timeDisplayLabel->setText(QString("%1s / %2s")
                                  .arg(m_currentTime, 0, 'f', 2)
                                  .arg(m_animationDuration, 0, 'f', 2));
}

template <typename Gadget>
QWidget *MainWindow::buildGadgetEditor(Gadget &g, QWidget *parent,
                                       std::function<void()> onChange) {
  const QMetaObject &mo = Gadget::staticMetaObject;
  auto *form = new QFormLayout();

  for (int i = mo.propertyOffset(); i < mo.propertyCount(); ++i) {
    QMetaProperty mp = mo.property(i);
    QVariant val = mp.readOnGadget(&g);

    QWidget *ed = makeEditor(
        parent, mp,
        /*setter*/
        [&, mp, onChange](QVariant v) {
          mp.writeOnGadget(&g, v);
          if (onChange)
            onChange();
        },
        val);

    form->addRow(QString::fromLatin1(mp.name()), ed);
  }

  auto *box = new QGroupBox(parent);
  box->setLayout(form);
  return box;
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

  QList<Entity> selected;
  for (auto &idx : sel.indexes())
    selected.append(m_sceneModel->getEntity(idx));
  m_canvas->setSelectedEntities(selected);
  m_canvas->update();

  if (selected.count() != 1) {
    return;
  }
  Entity e = selected.first();

  Scene &scene = m_canvas->scene(); // shorthand

  // ---------------------------------------------------------------- Name --
  if (auto *n = scene.reg.get<NameComponent>(e)) {
    auto *g = new QGroupBox(tr("Object"));
    auto *f = new QFormLayout(g);
    auto *edit = new QLineEdit(QString::fromStdString(n->name));
    f->addRow(tr("Name"), edit);

    connect(edit, &QLineEdit::textEdited, this, [this, e, edit] {
      if (auto *n = m_canvas->scene().reg.get<NameComponent>(e)) {
        std::string oldName = n->name;
        std::string newName = edit->text().toStdString();
        if (oldName != newName) {
          m_undoStack->push(new ChangeNameCommand(this, e, oldName, newName));
        }
      }
    });
    m_propsLayout->addRow(g);
  }

  // -------------------------------------------------------------- Shape --
  if (auto *s = scene.reg.get<ShapeComponent>(e)) {
    if (s->shape) {
      auto *grp = new QGroupBox(tr("Shape"));
      auto *lay = new QVBoxLayout(grp);
      lay->addWidget(new QLabel(s->shape->getKindName()));

      QWidget *editor = s->shape->createPropertyEditor(
          this, [this, e](QJsonObject new_props) {
            if (auto *sc = m_canvas->scene().reg.get<ShapeComponent>(e)) {
              QJsonObject old_props = sc->shape->serialize();
              m_undoStack->push(new ChangeShapePropertyCommand(
                  this, e, old_props, new_props));
              m_canvas->update();
            }
          });
      lay->addWidget(editor);
      m_propsLayout->addRow(grp);
    }
  }

  // -------------------------------------------------------- Transform --
  if (auto *tc = scene.reg.get<TransformComponent>(e)) {
    auto *g = new QGroupBox(tr("Transform"));
    auto *fl = new QFormLayout(g);

    auto addEditor = [&](const QString &label, const QString &objName,
                         auto getter, auto setter) {
      auto *spinbox = new QDoubleSpinBox(this);
      spinbox->setObjectName(objName);
      spinbox->setRange(-10000, 10000);
      spinbox->setDecimals(3);
      spinbox->setValue(getter(tc));

      // This flag tells us when to capture the component's "before" state.
      auto isNewInteraction = std::make_shared<bool>(true);

      // A place to store the state at the start of the interaction.
      auto stateOnBegin = std::make_shared<TransformComponent>();

      // When the user finishes typing and hits Enter or clicks away, we reset
      // the flag. This prepares us for the *next* separate interaction.
      connect(spinbox, &QDoubleSpinBox::editingFinished, this,
              [isNewInteraction]() { *isNewInteraction = true; });

      connect(spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
              this,
              [this, e, tc, setter, stateOnBegin,
               isNewInteraction](double currentValue) {
                // If this is the start of a new interaction (typing or
                // scrolling), capture the "before" state and set the flag to
                // false.
                if (*isNewInteraction) {
                  *stateOnBegin = *tc;
                  *isNewInteraction = false;
                }

                // Create what the "after" state will be.
                TransformComponent newState = *stateOnBegin;

                setter(newState, currentValue);
                m_isUpdatingFromUI = true;
                setter(*tc, currentValue);
                m_canvas->update();
                m_isUpdatingFromUI = false;

                // Push a new, mergeable command.
                // It correctly uses the state we captured at the very
                // beginning.
                m_undoStack->push(new ChangeTransformCommand(
                    this, e, stateOnBegin->x, stateOnBegin->y,
                    stateOnBegin->rotation, stateOnBegin->sx, stateOnBegin->sy,
                    newState.x, newState.y, newState.rotation, newState.sx,
                    newState.sy));
              });

      fl->addRow(label, spinbox);
    };

    // The getter/setter lambdas remain the same.
    addEditor(
        "X", "tx", [](auto *c) { return c->x; },
        [](auto &c, double v) { c.x = v; });
    addEditor(
        "Y", "ty", [](auto *c) { return c->y; },
        [](auto &c, double v) { c.y = v; });
    addEditor(
        "Rotation", "rot", [](auto *c) { return c->rotation * 180.0 / M_PI; },
        [](auto &c, double v) { c.rotation = v * M_PI / 180.0; });
    addEditor(
        "Scale X", "sx", [](auto *c) { return c->sx; },
        [](auto &c, double v) { c.sx = v; });
    addEditor(
        "Scale Y", "sy", [](auto *c) { return c->sy; },
        [](auto &c, double v) { c.sy = v; });

    m_propsLayout->addRow(g);
  } // --------------------------------------------------------- Material --
  if (auto *m = scene.reg.get<MaterialComponent>(e)) {
    auto *g = new QGroupBox(tr("Material"));
    auto *fl = new QFormLayout(g);

    auto *btn = new QPushButton();
    btn->setText("Color");
    QPalette pal(btn->palette());
    pal.setColor(QPalette::Button, QColor::fromRgba(m->color));
    btn->setPalette(pal);
    btn->setAutoFillBackground(true);

    connect(btn, &QPushButton::clicked, this, [this, e] {
      if (auto *mc = m_canvas->scene().reg.get<MaterialComponent>(e)) {
        QColor c = QColorDialog::getColor(QColor::fromRgba(mc->color), this);
        if (c.isValid() && c.rgba() != mc->color) {
          auto old_mc = *mc;
          auto new_mc = *mc;
          new_mc.color = c.rgba();
          m_undoStack->push(new ChangeMaterialCommand(
              this, e, old_mc.color, old_mc.isFilled, old_mc.isStroked,
              old_mc.strokeWidth, old_mc.antiAliased, new_mc.color,
              new_mc.isFilled, new_mc.isStroked, new_mc.strokeWidth,
              new_mc.antiAliased));
        }
      }
    });
    fl->addRow("Color", btn);

    auto makeCheck = [&](QString label, bool initial_value, auto setter) {
      auto *cb = new QCheckBox();
      cb->setChecked(initial_value);
      connect(cb, &QCheckBox::toggled, this, [this, e, setter](bool checked) {
        if (auto *mc = m_canvas->scene().reg.get<MaterialComponent>(e)) {
          auto old_mc = *mc;
          auto new_mc = setter(*mc, checked);
          m_undoStack->push(new ChangeMaterialCommand(
              this, e, old_mc.color, old_mc.isFilled, old_mc.isStroked,
              old_mc.strokeWidth, old_mc.antiAliased, new_mc.color,
              new_mc.isFilled, new_mc.isStroked, new_mc.strokeWidth,
              new_mc.antiAliased));
        }
      });
      fl->addRow(label, cb);
    };
    makeCheck("Filled", m->isFilled, [](auto mc, bool v) {
      mc.isFilled = v;
      return mc;
    });
    makeCheck("Stroked", m->isStroked, [](auto mc, bool v) {
      mc.isStroked = v;
      return mc;
    });
    makeCheck("AA", m->antiAliased, [](auto mc, bool v) {
      mc.antiAliased = v;
      return mc;
    });

    fl->addRow(
        "Stroke W",
        makeSpinBox<QDoubleSpinBox>(
            this, 0, 100, 0.1, [m] { return m->strokeWidth; },
            [this, e](double v) {
              if (auto *mc = m_canvas->scene().reg.get<MaterialComponent>(e)) {
                auto old_mc = *mc;
                auto new_mc = *mc;
                new_mc.strokeWidth = v;
                m_undoStack->push(new ChangeMaterialCommand(
                    this, e, old_mc.color, old_mc.isFilled, old_mc.isStroked,
                    old_mc.strokeWidth, old_mc.antiAliased, new_mc.color,
                    new_mc.isFilled, new_mc.isStroked, new_mc.strokeWidth,
                    new_mc.antiAliased));
              }
            }));
    m_propsLayout->addRow(g);
  }

  // ------------------------------------------------------- Animation --
  if (scene.reg.has<AnimationComponent>(e)) {
    auto *g = new QGroupBox(tr("Animation"));
    auto *fl = new QFormLayout(g);

    auto addEditor = [&](const QString &label, auto getter, auto setter) {
      auto *spinbox = makeSpinBox<QDoubleSpinBox>(
          this, 0, 1e3, 0.1,
          [this, e, getter] {
            if (auto *c = m_canvas->scene().reg.get<AnimationComponent>(e))
              return getter(c);
            return 0.0f;
          },
          [this, e, setter](double v) {
            if (auto *ac = m_canvas->scene().reg.get<AnimationComponent>(e)) {
              auto old_ac = *ac;
              auto new_ac = setter(*ac, v);
              if (memcmp(&old_ac, &new_ac, sizeof(AnimationComponent)) != 0) {
                m_undoStack->push(new ChangeAnimationCommand(
                    this, e, old_ac.entryTime, old_ac.exitTime,
                    new_ac.entryTime, new_ac.exitTime));
              }
            }
          });
      fl->addRow(label, spinbox);
    };

    addEditor(
        "Entry", [](auto c) { return c->entryTime; },
        [](auto c, double v) {
          c.entryTime = v;
          return c;
        });
    addEditor(
        "Exit", [](auto c) { return c->exitTime; },
        [](auto c, double v) {
          c.exitTime = v;
          return c;
        });
    m_propsLayout->addRow(g);
  }

  // ----------------------------------------------------------- Script --
  if (scene.reg.has<ScriptComponent>(e)) {
    auto *g = new QGroupBox(tr("Script"));
    auto *f = new QFormLayout(g);

    auto makeEdit = [&](const QString &lbl, auto getter, auto setter) {
      auto *le = new QLineEdit(QString::fromStdString(
          getter(m_canvas->scene().reg.get<ScriptComponent>(e))));
      f->addRow(lbl, le);
      connect(le, &QLineEdit::textEdited, this, [this, e, le, setter]() {
        if (auto *sc = m_canvas->scene().reg.get<ScriptComponent>(e)) {
          auto old_sc = *sc;
          auto new_sc = setter(*sc, le->text().toStdString());
          m_undoStack->push(new ChangeScriptCommand(
              this, e, old_sc.scriptPath, old_sc.startFunction,
              old_sc.updateFunction, old_sc.destroyFunction, new_sc.scriptPath,
              new_sc.startFunction, new_sc.updateFunction,
              new_sc.destroyFunction));
        }
      });
    };

    makeEdit(
        "Path", [](auto sc) { return sc->scriptPath; },
        [](auto sc, auto v) {
          sc.scriptPath = v;
          return sc;
        });
    makeEdit(
        "Start Func", [](auto sc) { return sc->startFunction; },
        [](auto sc, auto v) {
          sc.startFunction = v;
          return sc;
        });
    makeEdit(
        "Update Func", [](auto sc) { return sc->updateFunction; },
        [](auto sc, auto v) {
          sc.updateFunction = v;
          return sc;
        });
    makeEdit(
        "Destroy Func", [](auto sc) { return sc->destroyFunction; },
        [](auto sc, auto v) {
          sc.destroyFunction = v;
          return sc;
        });
    m_propsLayout->addRow(g);
  }
}

void MainWindow::syncTransformEditors(Entity e) {
  if (auto *tr = m_canvas->scene().reg.get<TransformComponent>(e)) {
    auto upd = [&](const char *name, double v) {
      if (auto *sb = findChild<QDoubleSpinBox *>(name)) {
        QSignalBlocker blk(sb); // avoid feedback
        sb->setValue(v);
      }
    };

    upd("tx", tr->x);
    upd("ty", tr->y);
    upd("rot", tr->rotation * 180.0 / M_PI);
    upd("sx", tr->sx);
    upd("sy", tr->sy);
  }
}

void MainWindow::onNewFile() {
  m_canvas->scene().clear();
  m_canvas->scene().createBackground(m_canvas->width(), m_canvas->height());
  captureInitialScene();
  m_sceneModel->refresh();
  m_canvas->update();
}

void MainWindow::onOpenFile() {
  QString filePath = QFileDialog::getOpenFileName(this, tr("Open Scene"), {},
                                                  tr("Scene Files (*.json)"));
  if (filePath.isEmpty())
    return;

  QFile loadFile(filePath);
  if (!loadFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open scene file.");
    return;
  }

  QJsonDocument doc(QJsonDocument::fromJson(loadFile.readAll()));
  m_canvas->scene().deserialize(doc.object());
  captureInitialScene();
  m_sceneModel->refresh();
  m_canvas->update();
}

void MainWindow::onSaveFile() {
  QString filePath = QFileDialog::getSaveFileName(this, tr("Save Scene"), {},
                                                  tr("Scene Files (*.json)"));
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

  Scene &scene = m_canvas->scene();
  for (const QJsonValue &v : doc.array()) {
    if (!v.isObject())
      continue;
    Entity e = scene.reg.create();
    ComponentRegistry::instance().apply(scene, e, v.toObject());
    m_undoStack->push(new AddEntityCommand(this, e));
  }

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
