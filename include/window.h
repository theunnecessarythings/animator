// #pragma once
//
// #include "canvas.h"
// #include "commands.h"
// #include "scene_model.h"
// #include "toolbox.h"
//
// #include <QAction>
// #include <QApplication>
// #include <QCheckBox>
// #include <QClipboard>
// #include <QColorDialog>
// #include <QCoreApplication>
// #include <QDockWidget>
// #include <QDoubleSpinBox>
// #include <QFileDialog>
// #include <QFormLayout>
// #include <QGroupBox>
// #include <QHBoxLayout>
// #include <QJsonDocument>
// #include <QJsonObject>
// #include <QLabel>
// #include <QLineEdit>
// #include <QListWidget>
// #include <QMainWindow>
// #include <QMenuBar>
// #include <QPushButton>
// #include <QSlider>
// #include <QSpinBox>
// #include <QTimer>
// #include <QTreeView>
// #include <QUndoStack>
// #include <QVBoxLayout>
//
// class MainWindow : public QMainWindow {
//   Q_OBJECT
// public:
//   explicit MainWindow(QWidget *parent = nullptr);
//   ~MainWindow() override;
//
//   SkiaCanvasWidget *canvas() const { return m_canvas; }
//   SceneModel *sceneModel() const { return m_sceneModel; }
//   QTreeView *sceneTree() const { return m_sceneTree; }
//   QUndoStack *undoStack() const { return m_undoStack; }
//
//   /** Take a JSON snapshot of the current scene (used for “Stop/Reset”) */
//   void captureInitialScene();
//
// private slots:
//   void onPlayPauseButtonClicked();
//   void onStopResetButtonClicked();
//   void onAnimationTimerTimeout();
//   void onTimelineSliderMoved(int value);
//   void updateTimeDisplay();
//
//   void onSceneSelectionChanged(const QItemSelection &selected,
//                                const QItemSelection &deselected);
//
//   void onNewFile();
//   void onOpenFile();
//   void onSaveFile();
//
//   void onTransformChanged(Entity entity);
//
//   void onCut();
//   void onDelete();
//   void onCopy();
//   void onPaste();
//
//   void onCanvasSelectionChanged(const QList<Entity> &entities);
//   void onTransformationCompleted(Entity entity, float oldX, float oldY,
//                                  float oldRotation, float newX, float newY,
//                                  float newRotation);
//
// private:
//   // --- helpers
//   ------------------------------------------------------------- void
//   createMenus(); void createToolbox(); void createSceneDock(); void
//   createPropertiesDock(); void createTimelineDock(); void clearLayout(QLayout
//   *layout); void resetScene(); // restore snapshot
//
//   void syncTransformEditors(Entity e);
//   template <typename Gadget>
//   QWidget *buildGadgetEditor(Gadget &g, QWidget *parent,
//                              std::function<void()> onChange);
//
//   // --- widgets & models
//   ---------------------------------------------------- SkiaCanvasWidget
//   *m_canvas = nullptr; SceneModel *m_sceneModel = nullptr; QTreeView
//   *m_sceneTree = nullptr; QFormLayout *m_propsLayout = nullptr;
//
//   QPushButton *m_playPauseButton = nullptr;
//   QPushButton *m_stopResetButton = nullptr;
//   QLabel *m_timeDisplayLabel = nullptr;
//   QSlider *m_timelineSlider = nullptr;
//   QTimer *m_animationTimer = nullptr;
//   QUndoStack *m_undoStack = nullptr;
//
//   // --- state
//   ---------------------------------------------------------------
//   QList<Entity> m_selectedEntities;
//   bool m_isPlaying = false;
//   float m_currentTime = 0.0f;
//   float m_animationDuration = 5.0f; // seconds
//   bool m_isUpdatingFromUI = false;
//   bool m_isDragging = false;
//
//   /* snapshot of the scene at launch / after Stop */
//   QJsonObject m_initialSceneJson;
// };

#pragma once

#include "canvas.h"
#include "commands.h"
#include "scene_model.h"
#include "toolbox.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
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
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QTreeView>
#include <QUndoStack>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  SkiaCanvasWidget *canvas() const { return m_canvas; }
  SceneModel *sceneModel() const { return m_sceneModel; }
  QTreeView *sceneTree() const { return m_sceneTree; }
  QUndoStack *undoStack() const { return m_undoStack; }

  /** Take a JSON snapshot of the current scene (used for “Stop/Reset”). */
  void captureInitialScene();

private slots:
  // Playback/Timeline
  void onPlayPauseButtonClicked();
  void onStopResetButtonClicked();
  void onAnimationTimerTimeout();
  void onTimelineSliderMoved(int value);
  void updateTimeDisplay();

  // Scene selection
  void onSceneSelectionChanged(const QItemSelection &selected,
                               const QItemSelection &deselected);

  // File actions
  void onNewFile();
  void onOpenFile();
  void onSaveFile();

  // Transform updates propagated from canvas
  void onTransformChanged(Entity entity);

  // Edit actions
  void onCut();
  void onDelete();
  void onCopy();
  void onPaste();

  // Canvas callbacks
  void onCanvasSelectionChanged(const QList<Entity> &entities);
  void onTransformationCompleted(Entity entity, float oldX, float oldY,
                                 float oldRotation, float newX, float newY,
                                 float newRotation);

private:
  // Helpers ------------------------------------------------------------------
  void createMenus();
  void createToolbox();
  void createSceneDock();
  void createPropertiesDock();
  void createTimelineDock();
  void clearLayout(QLayout *layout);
  void resetScene(); // restore snapshot
  void syncTransformEditors(Entity e);
  template <typename Gadget>
  QWidget *buildGadgetEditor(Gadget &g, QWidget *parent,
                             std::function<void()> onChange);

  // Widgets & models ---------------------------------------------------------
  SkiaCanvasWidget *m_canvas = nullptr;
  SceneModel *m_sceneModel = nullptr;
  QTreeView *m_sceneTree = nullptr;
  QFormLayout *m_propsLayout = nullptr;

  QPushButton *m_playPauseButton = nullptr;
  QPushButton *m_stopResetButton = nullptr;
  QLabel *m_timeDisplayLabel = nullptr;
  QSlider *m_timelineSlider = nullptr;
  QTimer *m_animationTimer = nullptr;
  QUndoStack *m_undoStack = nullptr;

  // State --------------------------------------------------------------------
  QList<Entity> m_selectedEntities;
  bool m_isPlaying = false;
  float m_currentTime = 0.0f;
  float m_animationDuration = 5.0f; // seconds
  bool m_isUpdatingFromUI = false;
  bool m_isDragging = false;

  /* snapshot of the scene at launch / after Stop */
  QJsonObject m_initialSceneJson;
};
