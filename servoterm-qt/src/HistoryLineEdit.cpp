#include "HistoryLineEdit.h"
#include <QKeyEvent>

static constexpr int MAX_HISTORY = 100;

HistoryLineEdit::HistoryLineEdit(QWidget *parent) : QLineEdit(parent) {}

void HistoryLineEdit::addToHistory(const QString &cmd) {
  if (cmd.isEmpty()) return;
  // Remove duplicate if it's the most recent entry.
  if (!m_history.isEmpty() && m_history.last() == cmd)
    m_history.removeLast();
  m_history.append(cmd);
  if (m_history.size() > MAX_HISTORY)
    m_history.removeFirst();
  m_pos = m_history.size();
}

void HistoryLineEdit::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Up) {
    if (m_history.isEmpty()) return;
    if (m_pos == m_history.size()) m_stash = text();
    if (m_pos > 0) {
      --m_pos;
      setText(m_history[m_pos]);
    }
    return;
  }
  if (event->key() == Qt::Key_Down) {
    if (m_pos < m_history.size()) {
      ++m_pos;
      setText(m_pos == m_history.size() ? m_stash : m_history[m_pos]);
    }
    return;
  }
  QLineEdit::keyPressEvent(event);
}
