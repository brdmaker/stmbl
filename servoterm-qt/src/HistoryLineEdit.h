#pragma once
#include <QLineEdit>
#include <QStringList>

// QLineEdit with up/down arrow command history (max 100 entries).
class HistoryLineEdit : public QLineEdit {
  Q_OBJECT
public:
  explicit HistoryLineEdit(QWidget *parent = nullptr);
  void addToHistory(const QString &cmd);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  QStringList m_history;
  int         m_pos  = 0;   // current browse position (m_history.size() = "at end")
  QString     m_stash;      // text saved when user starts browsing up
};
