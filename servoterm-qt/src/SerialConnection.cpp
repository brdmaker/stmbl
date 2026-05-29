#include "SerialConnection.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpSocket>
#include <QProcess>
#include <QTimer>

SerialConnection::SerialConnection(QObject *parent) : QObject(parent) {
  m_cfgTimer.setSingleShot(false);
  m_cfgTimer.setInterval(50);
  connect(&m_cfgTimer, &QTimer::timeout, this, &SerialConnection::onConfigTimer);
}

SerialConnection::~SerialConnection() { disconnect(); }

bool SerialConnection::isConnected() const {
  switch (m_mode) {
  case Serial: return m_serial && m_serial->isOpen();
  case Tcp:    return m_tcp    && m_tcp->state() == QAbstractSocket::ConnectedState;
  case Sim:    return m_sim    && m_sim->state() != QProcess::NotRunning;
  default:     return false;
  }
}

// --- connect/disconnect ---------------------------------------------------

void SerialConnection::connectSerial(const QString &portName) {
  disconnect();
  m_serial = new QSerialPort(portName, this);
  m_serial->setBaudRate(BAUD_RATE);
  m_serial->setDataBits(QSerialPort::Data8);
  m_serial->setParity(QSerialPort::NoParity);
  m_serial->setStopBits(QSerialPort::OneStop);
  m_serial->setFlowControl(QSerialPort::NoFlowControl);
  connect(m_serial, &QSerialPort::readyRead, this, &SerialConnection::onSerialReadyRead);
  connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
    if (e != QSerialPort::NoError)
      emit errorOccurred(m_serial->errorString());
  });
  if (!m_serial->open(QIODevice::ReadWrite)) {
    emit errorOccurred(m_serial->errorString());
    delete m_serial; m_serial = nullptr;
    return;
  }
  m_mode = Serial;
  emit connected();
}

void SerialConnection::connectTcp(const QString &host, quint16 port) {
  disconnect();
  m_tcp = new QTcpSocket(this);
  connect(m_tcp, &QTcpSocket::readyRead,   this, &SerialConnection::onTcpReadyRead);
  connect(m_tcp, &QTcpSocket::connected,   this, &SerialConnection::connected);
  connect(m_tcp, &QTcpSocket::disconnected, this, [this]{ m_mode = None; emit disconnected(); });
  connect(m_tcp, &QAbstractSocket::errorOccurred, this, [this]{
    emit errorOccurred(m_tcp->errorString());
  });
  m_tcp->connectToHost(host, port);
  m_mode = Tcp;
}

void SerialConnection::connectSim(const QString &stmblHostPath, const QString &script) {
  disconnect();
  m_sim = new QProcess(this);
  m_sim->setReadChannel(QProcess::StandardOutput);
  connect(m_sim, &QProcess::readyReadStandardOutput, this, &SerialConnection::onSimReadyRead);
  connect(m_sim, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus){ onSimFinished(code); });
  QStringList args;
  args << "--serve";
  if (!script.isEmpty()) args << script;
  m_sim->start(stmblHostPath, args);
  if (!m_sim->waitForStarted(3000)) {
    emit errorOccurred("Failed to start " + stmblHostPath);
    delete m_sim; m_sim = nullptr;
    return;
  }
  m_mode = Sim;
  emit connected();
}

void SerialConnection::disconnect() {
  m_cfgTimer.stop();
  m_cfgState = Idle;
  m_cfgQueue.clear();
  if (m_serial) { m_serial->close(); delete m_serial; m_serial = nullptr; }
  if (m_tcp)    { m_tcp->disconnectFromHost(); delete m_tcp; m_tcp = nullptr; }
  if (m_sim) {
    m_sim->write("exit\n");
    if (!m_sim->waitForFinished(1000)) m_sim->kill();
    delete m_sim; m_sim = nullptr;
  }
  if (m_mode != None) { m_mode = None; emit disconnected(); }
}

// --- send -----------------------------------------------------------------

void SerialConnection::sendCommand(const QString &cmd) {
  QString s = cmd;
  if (!s.endsWith('\n')) s += '\n';
  sendRaw(s.toUtf8());
}

void SerialConnection::sendRaw(const QByteArray &data) {
  switch (m_mode) {
  case Serial: if (m_serial) m_serial->write(data); break;
  case Tcp:    if (m_tcp)    m_tcp->write(data);    break;
  case Sim:    if (m_sim)    m_sim->write(data);    break;
  default: break;
  }
}

// --- available ports ------------------------------------------------------

QStringList SerialConnection::availablePorts() {
  QStringList result;
  for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
    // Prefer stmbl USB device; include all ports.
    result << info.portName();
  }
  return result;
}

// --- config protocol ------------------------------------------------------

void SerialConnection::loadConfig() {
  if (!isConnected()) return;
  m_cfgState      = Loading;
  m_cfgAccum.clear();
  m_cfgRedirecting = true;
  sendCommand("showconf");
  // 100 ms timeout: if no more data arrives we consider the dump complete.
  m_cfgTimer.start();
}

void SerialConnection::saveConfig(const QString &configText) {
  if (!isConnected()) return;
  m_cfgQueue.clear();
  m_cfgQueue.enqueue("deleteconf\n");
  for (const QString &line : configText.split('\n')) {
    QString cmd = "appendconf " + line + "\n";
    m_cfgQueue.enqueue(cmd.toUtf8());
  }
  m_cfgQueue.enqueue("flashsaveconf\n");
  m_cfgState = Saving;
  m_cfgTimer.start();
}

void SerialConnection::onConfigTimer() {
  if (m_cfgState == Loading) {
    // Timeout after no new data — config dump complete.
    m_cfgRedirecting = false;
    m_cfgTimer.stop();
    m_cfgState = Idle;
    emit configLoaded(m_cfgAccum);
  } else if (m_cfgState == Saving) {
    flushConfigQueue();
  }
}

void SerialConnection::flushConfigQueue() {
  if (m_cfgQueue.isEmpty()) {
    m_cfgTimer.stop();
    m_cfgState = Idle;
    return;
  }
  sendRaw(m_cfgQueue.dequeue());
}

// --- raw data handlers ----------------------------------------------------

void SerialConnection::handleRawData(const QByteArray &data) {
  if (m_cfgRedirecting && m_cfgState == Loading) {
    m_cfgAccum += QString::fromUtf8(data);
    // Restart timeout: more data still coming.
    m_cfgTimer.start();
    return;
  }
  emit dataReceived(data);
}

void SerialConnection::onSerialReadyRead() {
  handleRawData(m_serial->readAll());
}

void SerialConnection::onTcpReadyRead() {
  handleRawData(m_tcp->readAll());
}

void SerialConnection::onSimReadyRead() {
  handleRawData(m_sim->readAllStandardOutput());
}

void SerialConnection::onSimFinished(int) {
  m_mode = None;
  emit disconnected();
}
