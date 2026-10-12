#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <array>
#include "RTTYRx.h"

class RTTYScopeWidget final : public QWidget {
public:
  explicit RTTYScopeWidget(QWidget *parent = nullptr);
  void updateSample(const RTTY_RX_SCOPE &scope);
  void updateWaterfall(const float *samples, int count, double sampleRate,
                       double pitch, double shift, bool digu, bool carrier);
  void clear();
  QSize minimumSizeHint() const override;
protected:
  void paintEvent(QPaintEvent *) override;
private:
  struct Sample { float mark, space, x, y; };
  std::array<Sample, 512> points_{};
  std::array<std::array<float, 96>, 48> waterfall_{};
  int pos_ = 0, size_ = 0, wfPos_ = 0, wfSize_ = 0;
  unsigned wfFrames_ = 0;
  int noCarrierTicks_ = 0;
  bool carrier_ = false;
  float soft_ = 0.0f;
  double lowHz_ = 0, highHz_ = 0, markerA_ = 0, markerB_ = 0;
};
