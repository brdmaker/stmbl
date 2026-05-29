#include "Oscilloscope.h"
#include <QPainter>
#include <QResizeEvent>

const QColor Oscilloscope::CHANNEL_COLORS[SCOPE_CHANNELS] = {
  QColor(  0,   0,   0),  // black
  QColor(255,   0,   0),  // red
  QColor(  0,   0, 255),  // blue
  QColor(  0, 128,   0),  // green
  QColor(255, 128,   0),  // orange
  QColor(128, 128,   0),  // olive
  QColor(128,   0, 128),  // purple
  QColor(  0, 128, 128),  // teal
};

Oscilloscope::Oscilloscope(QWidget *parent) : QWidget(parent) {
  setMinimumSize(200, 100);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::white);
  setPalette(pal);
  setAutoFillBackground(true);
  resizeBuffers(m_width);
}

void Oscilloscope::resizeBuffers(int newWidth) {
  m_width = newWidth > 0 ? newWidth : 1;
  for (auto &ch : m_buf) { ch.resize(m_width); ch.fill(0.0f); }
  m_head = 0;
  m_full = false;
}

void Oscilloscope::resizeEvent(QResizeEvent *event) {
  resizeBuffers(event->size().width());
  QWidget::resizeEvent(event);
}

void Oscilloscope::addSample(const QVector<float> &channels) {
  for (int i = 0; i < SCOPE_CHANNELS && i < channels.size(); ++i)
    m_buf[i][m_head] = channels[i];
  m_head = (m_head + 1) % m_width;
  if (m_head == 0) m_full = true;
  update();
}

void Oscilloscope::reset() {
  resizeBuffers(m_width);
  update();
}

void Oscilloscope::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);

  const int W = width();
  const int H = height();
  const int nSamples = m_full ? m_width : m_head;
  if (nSamples < 2) return;

  // Mid-Y: value 0 maps to centre, ±1 maps to top/bottom.
  const float mid   = H / 2.0f;
  const float scale = H / 2.0f;

  for (int ch = 0; ch < SCOPE_CHANNELS; ++ch) {
    p.setPen(QPen(CHANNEL_COLORS[ch], 1));
    QPolygonF poly;
    poly.reserve(nSamples);
    for (int i = 0; i < nSamples; ++i) {
      int idx = m_full ? (m_head + i) % m_width : i;
      float x = (float)i / (float)(m_width - 1) * (W - 1);
      float y = mid - m_buf[ch][idx] * scale;
      poly << QPointF(x, y);
    }
    p.drawPolyline(poly);
  }

  // Centre line (light grey).
  p.setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
  p.drawLine(0, (int)mid, W, (int)mid);
}
