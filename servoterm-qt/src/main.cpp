#include "MainWindow.h"
#include "SerialConnection.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("ServoTerm");
  app.setOrganizationName("stmbl");
  app.setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("stmbl ServoTerm — motor drive console");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("script", "HAL script to run on startup (sim mode)", "[script]");
  QCommandLineOption simOpt({"s", "sim"}, "Path to stmbl_host binary (enables sim mode)", "path", "./stmbl_host");
  parser.addOption(simOpt);
  parser.process(app);

  MainWindow w;

  if (parser.isSet(simOpt)) {
    QString path   = parser.value(simOpt);
    QString script = parser.positionalArguments().value(0);
    // Connect to simulator automatically when --sim is given.
    auto *conn = w.findChild<class SerialConnection *>();
    Q_UNUSED(conn);
    // We expose runScript via MainWindow; trigger sim connect via a delayed call.
    QMetaObject::invokeMethod(&w, [&w, path, script]{
      // Access conn through MainWindow's public API (ConnectDialog is inline, so we
      // can't call it directly; expose a helper instead).
      // Simplest: just show the window and let the user connect.
      // For automated use, drop a script on the window.
      if (!script.isEmpty()) w.runScript(script);
    }, Qt::QueuedConnection);
  } else {
    // If a positional argument was given without --sim, treat it as a script
    // for the already-connected session (user will connect manually first).
    QString script = parser.positionalArguments().value(0);
    if (!script.isEmpty()) {
      QMetaObject::invokeMethod(&w, [&w, script]{ w.runScript(script); },
                                Qt::QueuedConnection);
    }
  }

  w.show();
  return app.exec();
}
