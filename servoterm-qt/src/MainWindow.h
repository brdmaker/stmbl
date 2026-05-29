#pragma once
#include <QMainWindow>
#include <QVector>
#include <QString>
#include <QTimer>

class QTextEdit;
class QPushButton;
class QComboBox;
class QLineEdit;
class QSplitter;
class QFile;

class SerialConnection;
class ScopeDataDemux;
class Oscilloscope;
class XYOscilloscope;
class HistoryLineEdit;
class ConfigDialog;
class Actions;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  // Open a script file on the connected simulator (drag-and-drop or CLI arg).
  void runScript(const QString &path);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private slots:
  void onConnectDialog();
  void onDisconnect();
  void onConnected();
  void onDisconnected();
  void onError(const QString &msg);
  void onDataReceived(const QByteArray &data);
  void onSendClicked();
  void onScopePacket(const QVector<float> &channels);
  void onScopeReset();
  void onEnableToggled(bool on);
  void onJogToggled(bool on);
  void onJogTimer();
  void onRecord(bool on);
  void onSetDirectory();
  void onOpenDirectory();
  void onPortComboClicked();
  void onEditConfig();
  void refreshPortList();

private:
  void setupUi();
  void setupMenus();
  void appendConsole(const QString &text);
  void saveSettings();
  void loadSettings();
  void updateConnectedState(bool connected);

  Actions          *m_actions   = nullptr;
  SerialConnection *m_conn      = nullptr;
  ScopeDataDemux   *m_demux     = nullptr;
  Oscilloscope     *m_scope     = nullptr;
  XYOscilloscope   *m_xyScope   = nullptr;
  QTextEdit        *m_console   = nullptr;
  HistoryLineEdit  *m_cmdLine   = nullptr;
  QPushButton      *m_sendBtn   = nullptr;
  QComboBox        *m_portCombo = nullptr;
  ConfigDialog     *m_cfgDialog = nullptr;

  // Jog
  enum JogState { JogIdle, JogCW, JogCCW };
  JogState m_jogState = JogIdle;
  QTimer   m_jogTimer;

  // CSV recording
  QString  m_recordDir;
  QFile   *m_csvFile   = nullptr;

  QString  m_textAccum;  // partial text line buffer
};
