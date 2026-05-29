#pragma once
#include <QDialog>
#include <QString>

class QTextEdit;
class QLabel;
class SerialConnection;

// Dialog for editing, loading, and saving stmbl config (showconf/flashsaveconf).
class ConfigDialog : public QDialog {
  Q_OBJECT
public:
  explicit ConfigDialog(SerialConnection *conn, QWidget *parent = nullptr);

  void loadFromDevice();

private slots:
  void onConfigLoaded(const QString &text);
  void onSave();
  void onTextChanged();

private:
  static quint32 crc32mpeg2(const QByteArray &data);

  SerialConnection *m_conn;
  QTextEdit        *m_editor;
  QLabel           *m_statusLabel;
};
