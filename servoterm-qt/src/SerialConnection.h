#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QQueue>

class QSerialPort;
class QTcpSocket;
class QProcess;

// Unified connection to stmbl device: serial port, TCP, or pipe to stmbl_host.
// Emits dataReceived() for every chunk of raw bytes received.
// Use sendCommand() to send a newline-terminated command.
// Config operations: loadConfig() / saveConfig().
class SerialConnection : public QObject {
  Q_OBJECT
public:
  explicit SerialConnection(QObject *parent = nullptr);
  ~SerialConnection() override;

  enum Mode { None, Serial, Tcp, Sim };
  Mode mode() const { return m_mode; }
  bool isConnected() const;

  // Connect / disconnect.
  void connectSerial(const QString &portName);
  void connectTcp(const QString &host, quint16 port);
  void connectSim(const QString &stmblHostPath, const QString &script = {});
  void disconnect();

  // Returns list of available serial port names.
  static QStringList availablePorts();

  // Send a raw string (newline appended if missing).
  void sendCommand(const QString &cmd);
  void sendRaw(const QByteArray &data);

  // Config protocol: read showconf → saveConfig(text).
  void loadConfig();
  void saveConfig(const QString &configText);

signals:
  void dataReceived(const QByteArray &data);
  void connected();
  void disconnected();
  void errorOccurred(const QString &msg);
  void configLoaded(const QString &text);

private slots:
  void onSerialReadyRead();
  void onTcpReadyRead();
  void onSimReadyRead();
  void onSimFinished(int exitCode);
  void onConfigTimer();

private:
  void handleRawData(const QByteArray &data);
  void flushConfigQueue();

  Mode        m_mode    = None;
  QSerialPort *m_serial = nullptr;
  QTcpSocket  *m_tcp    = nullptr;
  QProcess    *m_sim    = nullptr;

  // Config state machine.
  enum ConfigState { Idle, Loading, Saving };
  ConfigState  m_cfgState = Idle;
  QString      m_cfgAccum;
  bool         m_cfgRedirecting = false;
  QTimer       m_cfgTimer;
  QQueue<QByteArray> m_cfgQueue; // saveConfig lines to drip-feed

  static constexpr quint16 STMBL_VID = 0x0483;
  static constexpr quint16 STMBL_PID = 0x5740;
  static constexpr int     BAUD_RATE = 115200;
};
