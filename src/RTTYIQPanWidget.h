#pragma once
#include <QWidget>
#include <QByteArray>
#include <QImage>
#include <array>
#include <complex>
#include <functional>
#include <vector>

class RTTYIQPanWidget final : public QWidget {
public:
  explicit RTTYIQPanWidget(QWidget *parent = nullptr);
  void appendIQ(const QByteArray &data, unsigned sampleRate, unsigned channels, qsizetype offset, unsigned samples);
  void setFrequency(double hz);
  void setTones(double pitch, double shift, bool upper, bool reverse);
  void setFilter(double low, double high);
  void clearFilter();
  void reset();
  std::function<void(double)> clickTune;
  std::function<void(double, bool)> dragTune;
protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
private:
  static constexpr int N = 131072, Bins = 570, Rows = 76;
  void compute();
  std::vector<std::complex<float>> ring_, fft_;
  std::vector<float> window_;
  std::array<float, Bins> levels_{}, smooth_{}, dragLevels_{};
  QImage waterfall_;
  int write_ = 0, filled_ = 0, hop_ = 0, row_ = 0;
  unsigned rate_ = 0;
  bool ready_ = false, smoothed_ = false, displayFloorValid_ = false, waterFloorValid_ = false;
  float displayFloor_ = 0, waterFloor_ = 0;
  double frequency_ = 0, spaceOffset_ = -170, filterMark_ = -2210, filterLow_ = 0, filterHigh_ = 0;
  bool filterValid_ = false;
  bool tracking_ = false, dragging_ = false;
  double startX_ = 0, dragPixels_ = 0;
  qint64 lastSendMs_ = 0;
};
