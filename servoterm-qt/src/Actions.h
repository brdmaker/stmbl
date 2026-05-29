#pragma once
#include <QObject>

class QAction;

// All application QActions in one place.
class Actions : public QObject {
  Q_OBJECT
public:
  explicit Actions(QObject *parent = nullptr);

  // File
  QAction *fileQuit = nullptr;

  // Connection
  QAction *connectionConnect    = nullptr;
  QAction *connectionDisconnect = nullptr;

  // Drive
  QAction *driveEnable    = nullptr;  // checkable
  QAction *driveDisable   = nullptr;
  QAction *driveJogEnable = nullptr;  // checkable
  QAction *driveEditConfig = nullptr;

  // Data
  QAction *dataRecord       = nullptr;  // checkable
  QAction *dataSetDirectory = nullptr;
  QAction *dataOpenDirectory = nullptr;

  // View
  QAction *viewOscilloscope = nullptr;  // checkable
  QAction *viewXYScope      = nullptr;  // checkable
  QAction *viewConsole      = nullptr;  // checkable
  QAction *viewClearConsole = nullptr;
};
