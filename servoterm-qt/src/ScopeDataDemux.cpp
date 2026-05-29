#include "ScopeDataDemux.h"

ScopeDataDemux::ScopeDataDemux(QObject *parent)
  : QObject(parent), m_pending(8) {}

QString ScopeDataDemux::addData(const QByteArray &data) {
  QString text;
  for (unsigned char b : data) {
    switch (m_state) {
    case TEXT:
      if (b == 0xFF) {
        m_state = SCOPE_DATA;
        m_count = 0;
      } else if (b == 0xFE) {
        emit scopeResetReceived();
      } else {
        text += QChar(b);
      }
      break;
    case SCOPE_DATA:
      m_pending[m_count++] = (b - 128) / 128.0f;
      if (m_count == 8) {
        emit scopePacketReceived(m_pending);
        m_state = TEXT;
        m_count = 0;
      }
      break;
    }
  }
  return text;
}
