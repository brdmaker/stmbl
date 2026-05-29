#include "Actions.h"
#include <QAction>
#include <QApplication>

Actions::Actions(QObject *parent) : QObject(parent) {
  fileQuit = new QAction("&Quit", this);
  fileQuit->setShortcut(QKeySequence::Quit);

  connectionConnect    = new QAction("&Connect…",    this);
  connectionDisconnect = new QAction("&Disconnect",  this);
  connectionDisconnect->setEnabled(false);

  driveEnable = new QAction("&Enable",  this);
  driveEnable->setCheckable(true);
  driveDisable   = new QAction("&Disable", this);
  driveJogEnable = new QAction("&Jog",     this);
  driveJogEnable->setCheckable(true);
  driveEditConfig = new QAction("Edit &Config…", this);

  dataRecord        = new QAction("&Record",            this);
  dataRecord->setCheckable(true);
  dataSetDirectory   = new QAction("Set &Directory…",   this);
  dataOpenDirectory  = new QAction("&Open Directory",   this);

  viewOscilloscope = new QAction("&Oscilloscope", this);
  viewOscilloscope->setCheckable(true);
  viewOscilloscope->setChecked(true);
  viewXYScope      = new QAction("&XY Scope", this);
  viewXYScope->setCheckable(true);
  viewConsole      = new QAction("&Console",  this);
  viewConsole->setCheckable(true);
  viewConsole->setChecked(true);
  viewClearConsole = new QAction("C&lear Console", this);
}
