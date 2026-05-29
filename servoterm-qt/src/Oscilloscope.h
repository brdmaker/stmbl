#pragma once
#include <QWidget>
#include <QVector>
#include <array>

static constexpr int SCOPE_CHANNELS = 8;

// Scrolling oscilloscope widget: 8 channels, draws polylines per channel.
class Oscilloscope : public QWidget {
  Q_OBJECT
public:
  explicit Oscilloscope(QWidget *parent = nullptr);

public slots:
  void addSample(const QVector<float> &channels);
  void reset();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  // Ring buffer: one column per pixel width.
  int m_width  = 800;
  int m_head   = 0;   // next write position
  bool m_full  = false;
  std::array<QVector<float>, SCOPE_CHANNELS> m_buf;

  static const QColor CHANNEL_COLORS[SCOPE_CHANNELS];

  void resizeBuffers(int newWidth);
};
