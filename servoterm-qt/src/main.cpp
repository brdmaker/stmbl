#include "MainWindow.h"
#include "SerialConnection.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("ServoTerm");
  app.setOrganizationName("stmbl");
  app.setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("stmbl ServoTerm — motor drive console\n"
    "\n"
    "  Quick-start with built-in simulator:\n"
    "    servoterm --sim ../host/stmbl_host\n"
    "    servoterm --sim ../host/stmbl_host examples/motor.hal\n");
  parser.addHelpOption();
  parser.addVersionOption();
  // Positional: optional HAL script fed to the simulator on startup.
  parser.addPositionalArgument("script",
    "HAL/servoterm script to run at startup (sim mode only)", "[script]");
  // --sim / -s: path to stmbl_host binary; auto-connects on launch.
  QCommandLineOption simOpt({"s", "sim"},
    "Launch stmbl_host as simulator backend (enables sim mode)",
    "stmbl_host", "");
  parser.addOption(simOpt);
  parser.process(app);

  MainWindow w;
  w.show();

  if (parser.isSet(simOpt)) {
    QString path = parser.value(simOpt);
    // Default: look next to the servoterm binary, then in PATH.
    if (path.isEmpty()) {
      QString beside = QFileInfo(argv[0]).dir().filePath("stmbl_host");
      path = QFileInfo(beside).isExecutable() ? beside : "stmbl_host";
    }
    QString script = parser.positionalArguments().value(0);
    // Defer until the event loop is running so the window is fully shown.
    QMetaObject::invokeMethod(&w, [&w, path, script]{
      w.connectToSim(path, script);
    }, Qt::QueuedConnection);
  } else {
    QString script = parser.positionalArguments().value(0);
    if (!script.isEmpty()) {
      QMetaObject::invokeMethod(&w, [&w, script]{ w.runScript(script); },
                                Qt::QueuedConnection);
    }
  }

  return app.exec();
}
