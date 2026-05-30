#include "MainWindow.h"
#include "Actions.h"
#include "SerialConnection.h"
#include "ScopeDataDemux.h"
#include "Oscilloscope.h"
#include "XYOscilloscope.h"
#include "HistoryLineEdit.h"
#include "ConfigDialog.h"

#include <QApplication>
#include <QSplitter>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QMenuBar>
#include <QMenu>
#include <QLabel>
#include <QScrollBar>
#include <QStatusBar>
#include <QSettings>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>

// ---  ConnectDialog (inline) -------------------------------------------

class ConnectDialog : public QDialog {
public:
  enum ConnType { Serial, Tcp, Sim };
  ConnType     type;
  QString      value;   // portName, "host:port", or stmbl_host path
  QString      script;

  explicit ConnectDialog(QWidget *parent = nullptr) : QDialog(parent) {
    setWindowTitle("Connect");
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    auto *typeCombo = new QComboBox;
    typeCombo->addItems({"Serial port", "TCP", "Simulator (stmbl_host)"});
    form->addRow("Connection type:", typeCombo);

    auto *valueEdit = new QLineEdit;
    valueEdit->setPlaceholderText("e.g. /dev/ttyACM0 or COM3");
    form->addRow("Address / port:", valueEdit);

    // Populate serial ports in the combo.
    auto *portCombo = new QComboBox;
    portCombo->addItems(SerialConnection::availablePorts());
    form->addRow("Serial ports:", portCombo);

    auto *scriptEdit = new QLineEdit;
    scriptEdit->setPlaceholderText("Optional HAL script (sim mode only)");
    form->addRow("Script:", scriptEdit);

    layout->addLayout(form);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(portCombo, QOverload<int>::of(&QComboBox::activated), this,
      [valueEdit, portCombo](int idx){ valueEdit->setText(portCombo->itemText(idx)); });

    connect(this, &QDialog::accepted, this, [=]{
      script = scriptEdit->text().trimmed();
      switch (typeCombo->currentIndex()) {
      case 0: type = Serial; value = valueEdit->text().trimmed(); break;
      case 1: type = Tcp;    value = valueEdit->text().trimmed(); break;
      case 2: type = Sim;    value = valueEdit->text().trimmed();
              if (value.isEmpty()) value = "./stmbl_host";
              break;
      }
    });
  }
};

// --- MainWindow -----------------------------------------------------------

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  m_actions = new Actions(this);
  m_conn    = new SerialConnection(this);
  m_demux   = new ScopeDataDemux(this);

  setupUi();
  setupMenus();
  loadSettings();

  // Connections: serial connection → demux.
  connect(m_conn,  &SerialConnection::dataReceived, this, &MainWindow::onDataReceived);
  connect(m_conn,  &SerialConnection::connected,    this, &MainWindow::onConnected);
  connect(m_conn,  &SerialConnection::disconnected, this, &MainWindow::onDisconnected);
  connect(m_conn,  &SerialConnection::errorOccurred,this, &MainWindow::onError);

  // Demux → scope widgets + console.
  connect(m_demux, &ScopeDataDemux::scopePacketReceived, this, &MainWindow::onScopePacket);
  connect(m_demux, &ScopeDataDemux::scopeResetReceived,  this, &MainWindow::onScopeReset);

  // Actions.
  connect(m_actions->fileQuit,            &QAction::triggered,   qApp, &QApplication::quit);
  connect(m_actions->connectionConnect,   &QAction::triggered,   this, &MainWindow::onConnectDialog);
  connect(m_actions->connectionDisconnect,&QAction::triggered,   this, &MainWindow::onDisconnect);
  connect(m_actions->driveEnable,         &QAction::toggled,     this, &MainWindow::onEnableToggled);
  connect(m_actions->driveDisable,        &QAction::triggered,   this, [this]{ m_conn->sendCommand("1 sys0.en = 0"); });
  connect(m_actions->driveJogEnable,      &QAction::toggled,     this, &MainWindow::onJogToggled);
  connect(m_actions->driveEditConfig,     &QAction::triggered,   this, &MainWindow::onEditConfig);
  connect(m_actions->dataRecord,          &QAction::toggled,     this, &MainWindow::onRecord);
  connect(m_actions->dataSetDirectory,    &QAction::triggered,   this, &MainWindow::onSetDirectory);
  connect(m_actions->dataOpenDirectory,   &QAction::triggered,   this, &MainWindow::onOpenDirectory);
  connect(m_actions->viewOscilloscope,    &QAction::toggled,     m_scope,   &QWidget::setVisible);
  connect(m_actions->viewXYScope,         &QAction::toggled,     m_xyScope, &QWidget::setVisible);
  connect(m_actions->viewConsole,         &QAction::toggled,     m_console, &QWidget::setVisible);
  connect(m_actions->viewClearConsole,    &QAction::triggered,   m_console, &QTextEdit::clear);

  // Send button / line edit.
  connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
  connect(m_cmdLine, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

  // Jog timer (250 ms repeat).
  m_jogTimer.setInterval(250);
  connect(&m_jogTimer, &QTimer::timeout, this, &MainWindow::onJogTimer);

  setAcceptDrops(true);
  updateConnectedState(false);
}

MainWindow::~MainWindow() {
  saveSettings();
  if (m_csvFile) { m_csvFile->close(); delete m_csvFile; }
}

void MainWindow::setupUi() {
  auto *central = new QWidget(this);
  setCentralWidget(central);
  auto *mainLayout = new QVBoxLayout(central);
  mainLayout->setContentsMargins(4, 4, 4, 4);
  mainLayout->setSpacing(4);

  // Top: port selector row.
  auto *topRow = new QHBoxLayout;
  m_portCombo = new QComboBox(this);
  m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_portCombo->setToolTip("Click to refresh port list");
  connect(m_portCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int){
    // quick connect when port selected from list
  });
  topRow->addWidget(new QLabel("Port:"));
  topRow->addWidget(m_portCombo);
  auto *refreshBtn = new QPushButton("Refresh", this);
  connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPortList);
  topRow->addWidget(refreshBtn);
  mainLayout->addLayout(topRow);

  // Scope area.
  m_scope   = new Oscilloscope(this);
  m_xyScope = new XYOscilloscope(this);
  m_xyScope->setVisible(false);
  m_xyScope->setMinimumSize(200, 200);
  m_xyScope->setMaximumWidth(300);

  auto *scopeRow = new QHBoxLayout;
  scopeRow->addWidget(m_scope, 3);
  scopeRow->addWidget(m_xyScope, 1);
  mainLayout->addLayout(scopeRow, 2);

  // Console.
  m_console = new QTextEdit(this);
  m_console->setReadOnly(true);
  m_console->setFont(QFont("Monospace", 9));
  mainLayout->addWidget(m_console, 1);

  // Command input row.
  auto *cmdRow = new QHBoxLayout;
  m_cmdLine = new HistoryLineEdit(this);
  m_cmdLine->setPlaceholderText("Enter command…");
  m_sendBtn = new QPushButton("Send", this);
  m_sendBtn->setDefault(true);
  cmdRow->addWidget(m_cmdLine);
  cmdRow->addWidget(m_sendBtn);
  mainLayout->addLayout(cmdRow);

  setWindowTitle("ServoTerm");
  resize(900, 700);
  refreshPortList();
}

void MainWindow::setupMenus() {
  auto *file = menuBar()->addMenu("&File");
  file->addAction(m_actions->fileQuit);

  auto *conn = menuBar()->addMenu("&Connection");
  conn->addAction(m_actions->connectionConnect);
  conn->addAction(m_actions->connectionDisconnect);

  auto *drive = menuBar()->addMenu("&Drive");
  drive->addAction(m_actions->driveEnable);
  drive->addAction(m_actions->driveDisable);
  drive->addSeparator();
  drive->addAction(m_actions->driveJogEnable);
  drive->addSeparator();
  drive->addAction(m_actions->driveEditConfig);

  auto *data = menuBar()->addMenu("&Data");
  data->addAction(m_actions->dataRecord);
  data->addAction(m_actions->dataSetDirectory);
  data->addAction(m_actions->dataOpenDirectory);

  auto *view = menuBar()->addMenu("&View");
  view->addAction(m_actions->viewOscilloscope);
  view->addAction(m_actions->viewXYScope);
  view->addAction(m_actions->viewConsole);
  view->addSeparator();
  view->addAction(m_actions->viewClearConsole);
}

// --- event handlers -------------------------------------------------------

void MainWindow::onConnectDialog() {
  ConnectDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted) return;
  switch (dlg.type) {
  case ConnectDialog::Serial: m_conn->connectSerial(dlg.value); break;
  case ConnectDialog::Tcp: {
    QStringList parts = dlg.value.split(':');
    quint16 port = 5000;
    if (parts.size() == 2) port = parts[1].toUShort();
    m_conn->connectTcp(parts[0], port);
    break;
  }
  case ConnectDialog::Sim:
    m_conn->connectSim(dlg.value, dlg.script);
    break;
  }
}

void MainWindow::onDisconnect() {
  m_conn->disconnect();
}

void MainWindow::onConnected() {
  updateConnectedState(true);
  statusBar()->showMessage("Connected");
  appendConsole("<b>--- Connected ---</b>");
}

void MainWindow::onDisconnected() {
  updateConnectedState(false);
  statusBar()->showMessage("Disconnected");
  appendConsole("<b>--- Disconnected ---</b>");
  if (m_csvFile) { m_csvFile->close(); delete m_csvFile; m_csvFile = nullptr; }
  m_actions->dataRecord->setChecked(false);
}

void MainWindow::onError(const QString &msg) {
  statusBar()->showMessage("Error: " + msg);
  appendConsole("<span style='color:red'>Error: " + msg.toHtmlEscaped() + "</span>");
}

void MainWindow::onDataReceived(const QByteArray &data) {
  QString text = m_demux->addData(data);
  if (!text.isEmpty()) {
    m_textAccum += text;
    // Flush complete lines to console.
    int nl;
    while ((nl = m_textAccum.indexOf('\n')) >= 0) {
      QString line = m_textAccum.left(nl);
      m_textAccum  = m_textAccum.mid(nl + 1);
      appendConsole(line.toHtmlEscaped());
    }
  }
}

void MainWindow::onSendClicked() {
  QString cmd = m_cmdLine->text().trimmed();
  if (cmd.isEmpty()) return;
  m_cmdLine->addToHistory(cmd);
  m_cmdLine->clear();
  appendConsole("<span style='color:#005580'>&gt; " + cmd.toHtmlEscaped() + "</span>");
  m_conn->sendCommand(cmd);
}

void MainWindow::onScopePacket(const QVector<float> &channels) {
  m_scope->addSample(channels);
  if (m_xyScope->isVisible()) m_xyScope->addSample(channels);

  // CSV recording.
  if (m_csvFile && m_csvFile->isOpen()) {
    QTextStream ts(m_csvFile);
    for (int i = 0; i < channels.size(); ++i) {
      if (i > 0) ts << ',';
      ts << channels[i];
    }
    ts << '\n';
  }
}

void MainWindow::onScopeReset() {
  m_scope->reset();
  if (m_xyScope->isVisible()) m_xyScope->reset();
}

void MainWindow::onEnableToggled(bool on) {
  m_conn->sendCommand(on ? "1 sys0.en = 1" : "1 sys0.en = 0");
}

void MainWindow::onJogToggled(bool on) {
  if (!on) {
    m_jogTimer.stop();
    m_jogState = JogIdle;
  }
}

void MainWindow::onJogTimer() {
  switch (m_jogState) {
  case JogCW:  m_conn->sendCommand("jog 1");  break;
  case JogCCW: m_conn->sendCommand("jog -1"); break;
  default: break;
  }
}

void MainWindow::onRecord(bool on) {
  if (on) {
    if (m_recordDir.isEmpty()) onSetDirectory();
    if (m_recordDir.isEmpty()) { m_actions->dataRecord->setChecked(false); return; }
    QString fname = m_recordDir + "/scope_" +
                    QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
    m_csvFile = new QFile(fname, this);
    if (!m_csvFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
      onError("Cannot open CSV: " + fname);
      delete m_csvFile; m_csvFile = nullptr;
      m_actions->dataRecord->setChecked(false);
    } else {
      QTextStream ts(m_csvFile);
      ts << "ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7\n";
      statusBar()->showMessage("Recording: " + fname);
    }
  } else {
    if (m_csvFile) { m_csvFile->close(); delete m_csvFile; m_csvFile = nullptr; }
    statusBar()->showMessage("Recording stopped");
  }
}

void MainWindow::onSetDirectory() {
  QString dir = QFileDialog::getExistingDirectory(this, "Select recording directory", m_recordDir);
  if (!dir.isEmpty()) m_recordDir = dir;
}

void MainWindow::onOpenDirectory() {
  if (m_recordDir.isEmpty()) m_recordDir = QDir::homePath();
  QDesktopServices::openUrl(QUrl::fromLocalFile(m_recordDir));
}

void MainWindow::onPortComboClicked() { refreshPortList(); }

void MainWindow::onEditConfig() {
  if (!m_cfgDialog)
    m_cfgDialog = new ConfigDialog(m_conn, this);
  m_cfgDialog->loadFromDevice();
  m_cfgDialog->show();
  m_cfgDialog->raise();
}

void MainWindow::refreshPortList() {
  m_portCombo->clear();
  m_portCombo->addItems(SerialConnection::availablePorts());
}

// --- keyboard shortcut handling -------------------------------------------

void MainWindow::keyPressEvent(QKeyEvent *event) {
  // Escape = E-stop (send disable regardless of jog state).
  if (event->key() == Qt::Key_Escape) {
    m_conn->sendCommand("1 sys0.en = 0");
    m_actions->driveEnable->setChecked(false);
    m_jogTimer.stop();
    m_jogState = JogIdle;
    return;
  }
  if (m_actions->driveJogEnable->isChecked()) {
    if (event->key() == Qt::Key_Right) {
      if (m_jogState != JogCW) {
        m_jogState = JogCW;
        m_conn->sendCommand("jog 1");
        m_jogTimer.start();
      }
      return;
    }
    if (event->key() == Qt::Key_Left) {
      if (m_jogState != JogCCW) {
        m_jogState = JogCCW;
        m_conn->sendCommand("jog -1");
        m_jogTimer.start();
      }
      return;
    }
  }
  QMainWindow::keyPressEvent(event);
}

// --- drag-and-drop --------------------------------------------------------

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
  for (const QUrl &url : event->mimeData()->urls()) {
    QString path = url.toLocalFile();
    if (!path.isEmpty()) runScript(path);
  }
}

void MainWindow::runScript(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    onError("Cannot open script: " + path);
    return;
  }
  QTextStream ts(&f);
  while (!ts.atEnd()) {
    QString line = ts.readLine().trimmed();
    if (!line.isEmpty() && !line.startsWith('#'))
      m_conn->sendCommand(line);
  }
}

// --- sim launcher ---------------------------------------------------------

void MainWindow::connectToSim(const QString &stmblHostPath, const QString &script) {
  m_conn->connectSim(stmblHostPath, script);
  // Title shows which binary we launched.
  setWindowTitle("ServoTerm — " + stmblHostPath);
}

// --- helpers --------------------------------------------------------------

void MainWindow::appendConsole(const QString &html) {
  m_console->append(html);
  // Keep scroll at bottom.
  auto *sb = m_console->verticalScrollBar();
  sb->setValue(sb->maximum());
}

void MainWindow::updateConnectedState(bool connected) {
  m_actions->connectionConnect->setEnabled(!connected);
  m_actions->connectionDisconnect->setEnabled(connected);
  m_actions->driveEnable->setEnabled(connected);
  m_actions->driveDisable->setEnabled(connected);
  m_actions->driveJogEnable->setEnabled(connected);
  m_actions->driveEditConfig->setEnabled(connected);
  m_actions->dataRecord->setEnabled(connected);
  m_sendBtn->setEnabled(connected);
  m_cmdLine->setEnabled(connected);
}

void MainWindow::saveSettings() {
  QSettings s("stmbl", "servoterm");
  s.setValue("geometry",    saveGeometry());
  s.setValue("windowState", saveState());
  s.setValue("recordDir",   m_recordDir);
  s.setValue("lastPort",    m_portCombo->currentText());
  s.setValue("showXY",      m_actions->viewXYScope->isChecked());
  s.setValue("showConsole", m_actions->viewConsole->isChecked());
}

void MainWindow::loadSettings() {
  QSettings s("stmbl", "servoterm");
  if (s.contains("geometry"))    restoreGeometry(s.value("geometry").toByteArray());
  if (s.contains("windowState")) restoreState(s.value("windowState").toByteArray());
  m_recordDir = s.value("recordDir", QDir::homePath()).toString();
  bool showXY  = s.value("showXY",      false).toBool();
  bool showCon = s.value("showConsole", true).toBool();
  m_actions->viewXYScope->setChecked(showXY);
  m_xyScope->setVisible(showXY);
  m_actions->viewConsole->setChecked(showCon);
  m_console->setVisible(showCon);

  QString lastPort = s.value("lastPort").toString();
  if (!lastPort.isEmpty()) {
    int idx = m_portCombo->findText(lastPort);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  saveSettings();
  if (m_conn->isConnected()) m_conn->disconnect();
  QMainWindow::closeEvent(event);
}
