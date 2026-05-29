#pragma once
#include <QObject>
#include <QVector>

// Demuxes the stmbl binary wire protocol from a byte stream:
//   0xFF <b0> <b1> ... <b7>  — 8-channel scope sample (channel = (byte-128)/128.0)
//   0xFE                     — scope reset / new sweep
//   all other bytes          — text console output
class ScopeDataDemux : public QObject {
  Q_OBJECT
public:
  explicit ScopeDataDemux(QObject *parent = nullptr);

  // Feed raw bytes from the device/process; returns any accumulated text.
  QString addData(const QByteArray &data);

signals:
  void scopePacketReceived(QVector<float> channels);
  void scopeResetReceived();

private:
  enum State { TEXT, SCOPE_DATA };
  State      m_state   = TEXT;
  int        m_count   = 0;
  QVector<float> m_pending;
};
