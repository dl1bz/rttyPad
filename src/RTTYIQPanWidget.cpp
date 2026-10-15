#include "RTTYIQPanWidget.h"
#include <QDateTime>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr double pi = 3.14159265358979323846;
double i0(double x) {
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 80; ++k) {
    term *= (x*x/4.0)/(double(k)*k);
    sum += term;
    if (term < sum*1e-15) break;
  }
  return sum;
}
float median(std::array<float, 570> a) {
  std::sort(a.begin(), a.end());
  return 0.5f*(a[284]+a[285]);
}
QRgb waterfallColor(float v) {
  v = std::clamp(v, 0.0f, 1.0f);
  double r,g,b;
  if (v < .40f) { double t=v/.40; r=.01; g=.03+.17*t; b=.12+.45*t; }
  else if (v < .65f) { double t=(v-.40)/.25; r=.01; g=.20+.60*t; b=.57+.25*t; }
  else if (v < .85f) { double t=(v-.65)/.20; r=.01+.30*t; g=.80+.15*t; b=.82*(1-t); }
  else { double t=(v-.85)/.15; r=.31+.69*t; g=.95; b=0; }
  return qRgb(int(r*255),int(g*255),int(b*255));
}
float readFloat(const char *p) {
  // TCI binary payload is little-endian IEEE-754 float32.
  unsigned char b[4]; std::memcpy(b,p,4);
  quint32 bits = quint32(b[0]) | (quint32(b[1])<<8) | (quint32(b[2])<<16) | (quint32(b[3])<<24);
  float value; std::memcpy(&value,&bits,4); return value;
}
}
RTTYIQPanWidget::RTTYIQPanWidget(QWidget *parent) : QWidget(parent), ring_(N), fft_(N), window_(N), waterfall_(Bins,Rows,QImage::Format_RGB32) {
  setFixedSize(570,200);
  waterfall_.fill(QColor(2,8,31));
  const double denom=i0(14.0);
  for(int n=0;n<N;++n) { double t=2.0*n/(N-1)-1.0; window_[n]=float(i0(14.0*std::sqrt(std::max(0.0,1-t*t)))/denom); }
}
void RTTYIQPanWidget::reset() {
  write_=filled_=hop_=row_=0; rate_=0; ready_=smoothed_=displayFloorValid_=waterFloorValid_=false;
  waterfall_.fill(QColor(2,8,31)); update();
}
void RTTYIQPanWidget::setFrequency(double hz) { frequency_=hz; update(); }
void RTTYIQPanWidget::setTones(double pitch,double shift,bool upper,bool reverse) {
  filterMark_=upper?pitch:-pitch;
  spaceOffset_=(upper?1.0:-1.0)*(reverse?-1.0:1.0)*shift;
  update();
}
void RTTYIQPanWidget::setFilter(double low,double high) {
  if (std::isfinite(low)&&std::isfinite(high)&&low<high) { filterLow_=low; filterHigh_=high; filterValid_=true; update(); }
}
void RTTYIQPanWidget::clearFilter() { filterValid_=false; update(); }
void RTTYIQPanWidget::appendIQ(const QByteArray &data,unsigned rate,unsigned channels,qsizetype offset,unsigned samples) {
  if (channels!=2 || !rate || offset<0 || samples%2 || qsizetype(samples)*4>data.size()-offset) return;
  if (rate_!=rate) { reset(); rate_=rate; }
  const char *p=data.constData()+offset;
  const int stride=std::max(1,int(rate/10));
  for(unsigned n=0;n<samples/2;++n) {
    ring_[write_] = {readFloat(p+n*8),readFloat(p+n*8+4)};
    write_=(write_+1)%N;
    if(filled_<N) ++filled_;
    if(filled_==N && ++hop_>=stride) { hop_-=stride; compute(); }
  }
}
void RTTYIQPanWidget::compute() {
  for(int n=0;n<N;++n) fft_[n]=ring_[(write_+n)%N]*window_[n];
  // Forward complex FFT, in-place radix-2 DIT.
  for(int i=1,j=0;i<N;++i) {
    int bit=N>>1;
    for(;j&bit;bit>>=1) j^=bit;
    j^=bit;
    if(i<j) std::swap(fft_[i],fft_[j]);
  }
  for(int len=2;len<=N;len<<=1) {
    const std::complex<float> step=std::polar(1.0f,float(-2*pi/len));
    for(int i=0;i<N;i+=len) {
      std::complex<float> w(1,0);
      for(int j=0;j<len/2;++j) {
        auto a=fft_[i+j],b=fft_[i+j+len/2]*w;
        fft_[i+j]=a+b; fft_[i+j+len/2]=a-b; w*=step;
      }
    }
  }
  std::array<float,Bins> db{};
  for(int x=0;x<Bins;++x) {
    int bin=int(std::llround((double(x)/(Bins-1)-.5)*10000.0/rate_*N));
    bin=(bin%N+N)%N;
    db[x]=10.0f*std::log10(std::max(std::norm(fft_[bin]),1e-12f))-20.0f*std::log10(float(N));
  }
  float waterTarget=median(db)-12.0f;
  if(!waterFloorValid_) { waterFloor_=waterTarget; waterFloorValid_=true; }
  else waterFloor_+=.12f*(waterTarget-waterFloor_);
  for(int x=0;x<Bins;++x) {
    float level=std::clamp((db[x]-waterFloor_)/45.0f,0.0f,1.0f);
    waterfall_.setPixel(x,row_,waterfallColor(level));
  }
  row_=(row_+1)%Rows;
  if(!smoothed_) { smooth_=db; smoothed_=true; }
  else for(int x=0;x<Bins;++x) smooth_[x]+=.20f*(db[x]-smooth_[x]);
  float target=median(smooth_)-12.0f;
  if(!displayFloorValid_) { displayFloor_=target; displayFloorValid_=true; }
  else displayFloor_+=.12f*(target-displayFloor_);
  for(int x=0;x<Bins;++x) levels_[x]=std::clamp((smooth_[x]-displayFloor_)/60.0f,0.0f,1.0f);
  ready_=true; update();
}
void RTTYIQPanWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(),QColor(10,12,14));
  const int top=height()-23;
  const double split=height()-top*.30;
  const double spectrumTop=23.0;
  if(ready_) {
    // Oldest waterfall row is at the bottom; newest at the spectrum boundary.
    QImage arranged(Bins,Rows,QImage::Format_RGB32);
    for(int y=0;y<Rows;++y) {
      const int src=(row_+Rows-1-y)%Rows;
      std::memcpy(arranged.scanLine(y),waterfall_.constScanLine(src),Bins*4);
    }
    p.drawImage(QRectF(0,split,width(),height()-split),arranged);
    p.save(); p.setClipRect(QRectF(0,spectrumTop,width(),split-spectrumTop));
    p.setPen(QPen(QColor(166,166,166,102),1));
    for(int k=-4;k<=4;++k) { double x=(.5+k/10.0)*(width()-1); p.drawLine(QPointF(x,spectrumTop),QPointF(x,split)); }
    for(int db=10;db<60;db+=10) { double y=split-(split-spectrumTop)*db/60.0; p.drawLine(QPointF(0,y),QPointF(width(),y)); }
    if(filterValid_) {
      double x0=(.5+(filterLow_-filterMark_)/10000.0)*(width()-1);
      double x1=(.5+(filterHigh_-filterMark_)/10000.0)*(width()-1);
      p.fillRect(QRectF(x0,spectrumTop,x1-x0,split-spectrumTop),QColor(115,153,191,46));
    }
    QPainterPath shape,trace;
    const auto &v=dragging_?dragLevels_:levels_;
    double dx=dragging_?dragPixels_:0.0;
    for(int x=0;x<Bins;++x) {
      QPointF point(double(x)*(width()-1)/(Bins-1)+dx,split-v[x]*(split-spectrumTop));
      if(x==0) trace.moveTo(point); else trace.lineTo(point);
    }
    shape=trace; shape.lineTo(double(Bins-1)*(width()-1)/(Bins-1)+dx,split); shape.lineTo(dx,split); shape.closeSubpath();
    QLinearGradient gradient(0,split,0,spectrumTop);
    gradient.setColorAt(0,QColor(9,89,13)); gradient.setColorAt(.15,QColor(38,230,26));
    gradient.setColorAt(.40,QColor(255,230,13)); gradient.setColorAt(.65,QColor(255,110,8));
    gradient.setColorAt(.85,QColor(242,26,13)); gradient.setColorAt(1,QColor(242,26,13));
    p.fillPath(shape,gradient);
    p.setPen(QPen(QColor(64,242,64),1)); p.drawPath(trace);
    p.restore();
  } else {
    p.setPen(Qt::gray); p.drawText(12,height()/2,QStringLiteral("Waiting for TCI I/Q…"));
  }
  p.save(); p.setClipRect(QRectF(0,spectrumTop,width(),split-spectrumTop));
  p.setPen(QPen(QColor(38,242,89,217),1));
  p.drawLine(QPointF((width()-1)/2.0,spectrumTop),QPointF((width()-1)/2.0,split));
  p.setPen(QPen(QColor(255,204,38,217),1));
  double spaceX=(.5+spaceOffset_/10000.0)*(width()-1);
  p.drawLine(QPointF(spaceX,spectrumTop),QPointF(spaceX,split));
  p.restore();
  QFont font=QFontDatabase::systemFont(QFontDatabase::FixedFont); font.setPointSize(10);
  p.setFont(font); p.setPen(QColor(190,190,190));
  p.drawText(QRect(8,0,190,22),Qt::AlignVCenter,QStringLiteral("TCI I/Q  •  10.00 kHz"));
  if(frequency_>0) {
    p.setPen(Qt::white);
    p.drawText(QRect(width()/2-95,0,190,22),Qt::AlignCenter,QStringLiteral("%1 MHz").arg(frequency_/1e6,0,'f',6));
  }
}
void RTTYIQPanWidget::mousePressEvent(QMouseEvent *e) {
  if(e->button()!=Qt::LeftButton || !ready_) return;
  if(e->position().y()<23 || e->position().y()>height()-double(height()-23)*.30) return;
  tracking_=true; dragging_=false; startX_=e->position().x(); lastSendMs_=0;
}
void RTTYIQPanWidget::mouseMoveEvent(QMouseEvent *e) {
  if(!tracking_) return;
  double dx=e->position().x()-startX_;
  if(!dragging_) {
    if(std::fabs(dx)<3) return;
    dragging_=true; dragLevels_=levels_;
    if(dragTune) dragTune(0,true);
  }
  dragPixels_=dx; update();
  qint64 now=QDateTime::currentMSecsSinceEpoch();
  if(now-lastSendMs_>=40) { if(dragTune) dragTune(dx*10000.0/(width()-1),false); lastSendMs_=now; }
}
void RTTYIQPanWidget::mouseReleaseEvent(QMouseEvent *e) {
  if(!tracking_ || e->button()!=Qt::LeftButton) return;
  tracking_=false;
  double dx=e->position().x()-startX_;
  if(dragging_) {
    dragging_=false; dragPixels_=0; update();
    if(dragTune) dragTune(dx*10000.0/(width()-1),false);
  } else if(clickTune) clickTune((std::clamp(e->position().x(),0.0,double(width()-1))/(width()-1)-.5)*10000.0);
}
