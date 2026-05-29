#pragma once
#include <QWidget>
#include <QVector>
#include <QTimer>
#include <QPointF>
#include <QColor>

struct XYPoint { float x, y; QColor color; };

// XY persistence oscilloscope: ch0=X, ch1=Y.
// Points fade to white over time (50 ms timer increments RGB toward 255).
class XYOscilloscope : public QWidget {
  Q_OBJECT
public:
  explicit XYOscilloscope(QWidget *parent = nullptr);

public slots:
  void addSample(const QVector<float> &channels);
  void reset();

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void fade();

private:
  QVector<XYPoint> m_points;
  QTimer           m_fadeTimer;

  static constexpr int FADE_STEP  = 8;   // RGB increase per tick toward 255
  static constexpr int FADE_MS    = 50;
  static constexpr int MAX_POINTS = 4096;
};
