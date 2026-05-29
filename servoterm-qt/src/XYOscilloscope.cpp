#include "XYOscilloscope.h"
#include <QPainter>

XYOscilloscope::XYOscilloscope(QWidget *parent) : QWidget(parent) {
  setMinimumSize(100, 100);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::white);
  setPalette(pal);
  setAutoFillBackground(true);

  connect(&m_fadeTimer, &QTimer::timeout, this, &XYOscilloscope::fade);
  m_fadeTimer.start(FADE_MS);
}

void XYOscilloscope::addSample(const QVector<float> &channels) {
  if (channels.size() < 2) return;
  if (m_points.size() >= MAX_POINTS) m_points.removeFirst();
  m_points.append({ channels[0], channels[1], QColor(0, 0, 0) });
  update();
}

void XYOscilloscope::reset() {
  m_points.clear();
  update();
}

void XYOscilloscope::fade() {
  bool changed = false;
  for (auto &pt : m_points) {
    int r = qMin(255, pt.color.red()   + FADE_STEP);
    int g = qMin(255, pt.color.green() + FADE_STEP);
    int b = qMin(255, pt.color.blue()  + FADE_STEP);
    pt.color = QColor(r, g, b);
    changed = true;
  }
  // Remove fully faded (white) points.
  m_points.erase(
    std::remove_if(m_points.begin(), m_points.end(),
      [](const XYPoint &p){ return p.color == QColor(255, 255, 255); }),
    m_points.end());
  if (changed) update();
}

void XYOscilloscope::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);

  const int W = width();
  const int H = height();
  const float midX = W / 2.0f;
  const float midY = H / 2.0f;
  const float scaleX = W / 2.0f;
  const float scaleY = H / 2.0f;

  for (const auto &pt : m_points) {
    p.setPen(pt.color);
    float px = midX + pt.x * scaleX;
    float py = midY - pt.y * scaleY;
    p.drawPoint(QPointF(px, py));
  }
}
