#include "ConfigDialog.h"
#include "SerialConnection.h"

#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

ConfigDialog::ConfigDialog(SerialConnection *conn, QWidget *parent)
  : QDialog(parent), m_conn(conn) {
  setWindowTitle("Configuration");
  setMinimumSize(500, 400);

  m_editor = new QTextEdit(this);
  m_editor->setFont(QFont("Monospace", 10));
  connect(m_editor, &QTextEdit::textChanged, this, &ConfigDialog::onTextChanged);

  m_statusLabel = new QLabel("Size: 0 bytes   CRC32: 0x00000000", this);

  auto *btnBox = new QDialogButtonBox(this);
  auto *loadBtn = btnBox->addButton("Reload from device", QDialogButtonBox::ResetRole);
  auto *saveBtn = btnBox->addButton("Save to device",     QDialogButtonBox::AcceptRole);
  auto *closeBtn = btnBox->addButton(QDialogButtonBox::Close);

  connect(loadBtn,  &QPushButton::clicked, this, &ConfigDialog::loadFromDevice);
  connect(saveBtn,  &QPushButton::clicked, this, &ConfigDialog::onSave);
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

  connect(m_conn, &SerialConnection::configLoaded, this, &ConfigDialog::onConfigLoaded);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_editor);
  layout->addWidget(m_statusLabel);
  layout->addWidget(btnBox);
}

void ConfigDialog::loadFromDevice() {
  m_statusLabel->setText("Loading…");
  m_conn->loadConfig();
}

void ConfigDialog::onConfigLoaded(const QString &text) {
  m_editor->blockSignals(true);
  m_editor->setPlainText(text);
  m_editor->blockSignals(false);
  onTextChanged();
}

void ConfigDialog::onSave() {
  m_conn->saveConfig(m_editor->toPlainText());
}

void ConfigDialog::onTextChanged() {
  QByteArray data = m_editor->toPlainText().toUtf8();
  quint32 crc = crc32mpeg2(data);
  m_statusLabel->setText(
    QString("Size: %1 bytes   CRC32: 0x%2")
      .arg(data.size())
      .arg(crc, 8, 16, QLatin1Char('0')));
}

// CRC-32/MPEG-2: poly 0x04C11DB7, init 0xFFFFFFFF, no reflection.
quint32 ConfigDialog::crc32mpeg2(const QByteArray &data) {
  quint32 crc = 0xFFFFFFFFu;
  for (unsigned char b : data) {
    crc ^= (quint32)b << 24;
    for (int i = 0; i < 8; ++i)
      crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : (crc << 1);
  }
  return crc;
}
