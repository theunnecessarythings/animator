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
  explicit MainWindow(QWidget *parent = nullptr);

  ~MainWindow() {
    disconnect(m_sceneTree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSceneSelectionChanged);
    m_canvas->scene().clear();
  }

  SkiaCanvasWidget *canvas() const { return m_canvas; }
  SceneModel *sceneModel() const { return m_sceneModel; }
  QTreeView *sceneTree() const { return m_sceneTree; }
  QUndoStack *undoStack() const { return m_undoStack; }
  void setInitialRegistry(const Registry &reg) { m_initialRegistry = reg; }

private slots:
  void onPlayPauseButtonClicked();
  void onStopResetButtonClicked();
  void onAnimationTimerTimeout();
  void onTimelineSliderMoved(int value);
  void updateTimeDisplay();
  void onSceneSelectionChanged(const QItemSelection &selected,
                               const QItemSelection &deselected);
  void onNewFile();
  void onOpenFile();
  void onSaveFile();
  void onTransformChanged(Entity entity);
  void onCut();
  void onDelete();
  void onCopy();
  void onPaste();
  void onCanvasSelectionChanged(const QList<Entity> &entities);
  void onTransformationCompleted(Entity entity, float oldX, float oldY,
                                 float oldRotation, float newX, float newY,
                                 float newRotation);

private:
  void createMenus();
  void createToolbox();
  void createSceneDock();
  void createPropertiesDock();
  void createTimelineDock();
  void clearLayout(QLayout *layout);
  void resetScene();
  template <typename Gadget>
  QWidget *buildGadgetEditor(Gadget &g, QWidget *parent,
                             std::function<void()> onChange);

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
  QList<Entity> m_selectedEntities;
  bool m_isPlaying = false;
  float m_currentTime = 0.0f;
  float m_animationDuration = 5.0f; // Default animation duration
  bool m_isUpdatingFromUI = false;
};
