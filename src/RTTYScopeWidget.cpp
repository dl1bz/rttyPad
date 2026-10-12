#include "RTTYScopeWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace {
constexpr double pi = 3.14159265358979323846;
double clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
QColor green(int alpha = 255) { return QColor(115, 255, 115, alpha); }
}

RTTYScopeWidget::RTTYScopeWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(420, 200);
}
QSize RTTYScopeWidget::minimumSizeHint() const { return QSize(420, 200); }
void RTTYScopeWidget::clear()
{
    pos_ = size_ = wfPos_ = wfSize_ = noCarrierTicks_ = 0;
    wfFrames_ = 0;
    carrier_ = false;
    soft_ = 0;
    lowHz_ = highHz_ = markerA_ = markerB_ = 0;
    for (auto &row : waterfall_) row.fill(0);
    update();
}
void RTTYScopeWidget::updateSample(const RTTY_RX_SCOPE &scope)
{
    carrier_ = scope.carrier != 0;
    soft_ = carrier_ ? scope.soft : 0;
    if (carrier_) {
        noCarrierTicks_ = 0;
        points_[pos_] = {float(clamp(scope.mark_mag, 0, 1)),
                         float(clamp(scope.space_mag, 0, 1)),
                         float(clamp(scope.xy_mark, -1, 1)),
                         float(clamp(scope.xy_space, -1, 1))};
        pos_ = (pos_ + 1) % int(points_.size());
        size_ = std::min(size_ + 1, int(points_.size()));
    } else if (++noCarrierTicks_ > 24) {
        pos_ = size_ = 0;
    }
    update();
}
void RTTYScopeWidget::updateWaterfall(const float *samples, int count, double rate,
                                      double pitch, double shift, bool digu, bool carrier)
{
    if (++wfFrames_ % 8 != 1) return; // AppKit: one row per eight audio blocks
    const double a = digu ? pitch - shift : pitch;
    const double b = digu ? pitch : pitch + shift;
    markerA_ = std::min(a, b);
    markerB_ = std::max(a, b);
    const double nyquist = rate > 0 ? rate * 0.5 : 24000;
    lowHz_ = clamp(markerA_ - 280, 50, nyquist - 100);
    highHz_ = clamp(markerB_ + 280, lowHz_ + 100, nyquist - 10);
    auto &row = waterfall_[wfPos_];
    row.fill(0);
    if (carrier && samples && count > 16 && rate > 0) {
        std::array<double, 96> power{};
        double maxPower = 1e-18;
        for (int bin = 0; bin < 96; ++bin) {
            const double freq = lowHz_ + double(bin) / 95.0 * (highHz_ - lowHz_);
            const double coeff = 2 * std::cos(2 * pi * freq / rate);
            double q1 = 0, q2 = 0;
            for (int n = 0; n < count; ++n) {
                const double window = 0.5 - 0.5 * std::cos(2 * pi * n / (count - 1));
                const double q0 = samples[n] * window + coeff * q1 - q2;
                q2 = q1;
                q1 = q0;
            }
            power[bin] = std::max(0.0, q1*q1 + q2*q2 - coeff*q1*q2);
            maxPower = std::max(maxPower, power[bin]);
        }
        for (int bin = 0; bin < 96; ++bin) {
            const double v = clamp(power[bin] / maxPower, 0, 1);
            row[bin] = v < 0.025 ? 0 : float(std::pow(v, 1.15));
        }
    }
    wfPos_ = (wfPos_ + 1) % 48;
    wfSize_ = std::min(wfSize_ + 1, 48);
}
void RTTYScopeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 5, 0));
    const double pad = 8, gap = 8;
    const double h = std::max(1.0, height() - 2*pad);
    const double xySize = std::max(72.0, std::min(h, double(width()) - 2*pad - gap - 150));
    const double leftW = std::max(96.0, width() - 2*pad - gap - xySize);
    const double spectrumH = std::floor((h - gap) * 0.54);
    QRectF spec(pad, pad, leftW, spectrumH);
    QRectF wf(pad, pad + spectrumH + gap, leftW, h - spectrumH - gap);
    QRectF xy(pad + leftW + gap, pad + std::floor((h - xySize)/2), xySize, xySize);
    p.setPen(QPen(QColor(0, 92, 0), 1));
    p.setBrush(QColor(0, 2, 0));
    p.drawRect(spec); p.drawRect(wf); p.drawRect(xy);
    const QRectF sp = spec.adjusted(6, 12, -6, -12);
    const QRectF wp = wf.adjusted(6, 6, -6, -6);
    const double baseY = sp.bottom() - 8;
    const bool freqOK = highHz_ > lowHz_ && markerA_ > 0 && markerB_ > 0;
    const auto target = [&](double hz, const QRectF &r, double fallback) {
        return r.left() + r.width() * (freqOK ? clamp((hz - lowHz_)/(highHz_-lowHz_), 0, 1) : fallback);
    };
    const double sx = target(markerA_, sp, 0.42), mx = target(markerB_, sp, 0.70);
    p.setPen(QPen(QColor(0, 42, 0), 1)); p.drawLine(QPointF(sp.left(), baseY), QPointF(sp.right(), baseY));
    p.setPen(QPen(QColor(0, 158, 0), 1));
    p.drawLine(QPointF(sx, baseY), QPointF(sx, sp.top()+2));
    p.drawLine(QPointF(mx, baseY), QPointF(mx, sp.top()+2));
    if (carrier_ && size_) {
        const Sample &last = points_[(pos_ + 511) % 512];
        const double sh = 8 + last.space * (sp.height()-12)*0.92;
        const double mh = 8 + last.mark * (sp.height()-12)*0.92;
        QPainterPath curve;
        for (int i=0; i<=160; ++i) {
            const double x = sp.left() + double(i)/160 * sp.width();
            const double ds = (x-sx)/(sp.width()*0.038), dm = (x-mx)/(sp.width()*0.038);
            const double ks = (x-sx)/(sp.width()*0.095), km = (x-mx)/(sp.width()*0.095);
            const double v = 3.5 + sh*std::exp(-0.5*ds*ds) + mh*std::exp(-0.5*dm*dm)
                            + 0.18*sh*std::exp(-0.5*ks*ks) + 0.18*mh*std::exp(-0.5*km*km);
            if (i==0) curve.moveTo(x,baseY-v); else curve.lineTo(x,baseY-v);
        }
        p.setPen(QPen(QColor(235,255,224),1.15)); p.drawPath(curve);
    }
    const double pointerX = (sx+mx)/2;
    QPolygonF triangle{QPointF(pointerX-5,baseY+3),QPointF(pointerX+5,baseY+3),QPointF(pointerX,baseY-6)};
    p.setPen(Qt::NoPen); p.setBrush(QColor(255,209,61)); p.drawPolygon(triangle);
    p.fillRect(wp, QColor(3, 8, 38));
    for (int row=0; row<48; ++row) {
        const double y = wp.top() + row*wp.height()/48;
        p.fillRect(QRectF(wp.left(),y,wp.width(),wp.height()/48), QColor(3,row%2?15:10,38,200));
    }
    for (int row=0; row<wfSize_; ++row) {
        const int index = (wfPos_ + 47 - row)%48;
        const double y = wp.bottom() - (row+1)*wp.height()/48;
        const double age = double(row)/48;
        const double historyAlpha = 0.30 + (1-age)*0.70;
        for (int bin=0; bin<96; ++bin) {
            const double v = waterfall_[index][bin];
            if (v<=0.055) continue;
            const double hot = std::max(0.0,(v-0.74)/0.26);
            const double alpha = historyAlpha * std::min(1.0,0.18+v*0.95);
            QColor c; c.setRgbF(clamp(0.03+hot*0.74,0,1),clamp(0.36+v*0.70,0,1),clamp(0.22+v*0.30,0,1),clamp(alpha,0,1));
            const double binW = wp.width()/96;
            const double drawW = std::max(0.75,binW*0.48);
            p.fillRect(QRectF(wp.left()+(bin+0.5)*binW-drawW/2,y,drawW,wp.height()/48),c);
        }
    }
    p.setPen(QPen(QColor(0,184,0,220),1));
    for (double hz : {markerA_, markerB_}) {
        const double x = target(hz,wp,hz==markerA_?0.42:0.70);
        p.drawLine(QPointF(x,wp.top()),QPointF(x,wp.bottom()));
    }
    QRectF xp = xy.adjusted(12,14,-12,-14);
    p.setPen(QPen(QColor(0,23,0),0.8)); p.drawLine(xp.bottomLeft(),xp.topRight());
    p.setPen(QPen(QColor(0,46,0),1));
    p.drawLine(xp.bottomLeft(),xp.bottomRight()); p.drawLine(xp.bottomLeft(),xp.topLeft());
    const QPointF center = xp.center();
    const double scale = std::min(xp.width(),xp.height())*0.44;
    QPointF previous;
    bool havePrevious=false;
    const int trace = carrier_ ? std::min(size_,480) : 0;
    for (int j=0; j<trace; ++j) {
        const int idx = (pos_ - trace + j + 512)%512;
        const Sample &s = points_[idx];
        const QPointF pt(center.x()+s.x*scale,center.y()-s.y*scale);
        const double age = double(j+1)/trace;
        if (havePrevious && std::hypot(pt.x()-previous.x(),pt.y()-previous.y())<scale*0.24) {
            p.setPen(QPen(QColor(26,255,46,int(255*(0.030+age*0.10))),0.48));
            p.drawLine(previous,pt);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0,245,51,int(255*(0.040+age*0.095))));
        p.drawEllipse(pt,1.1+age*1.25,1.1+age*1.25);
        p.setBrush(QColor(122,255,122,int(255*(0.22+age*0.60))));
        p.drawEllipse(pt,0.55+age*0.62,0.55+age*0.62);
        previous=pt; havePrevious=true;
    }
    for (int j=0; j<std::min(trace,36); ++j) {
        const int hotCount=std::min(trace,36);
        const int idx=(pos_-hotCount+j+512)%512;
        const Sample &s=points_[idx];
        p.setBrush(QColor(199,255,199,int(255*(0.30+double(j+1)/hotCount*0.62))));
        p.drawEllipse(QPointF(center.x()+s.x*scale,center.y()-s.y*scale),1.05,1.05);
    }
    p.setPen(green());
    p.drawText(spec.adjusted(8,4,0,0),Qt::AlignTop|Qt::AlignLeft,QStringLiteral("Spectrum"));
    p.drawText(wf.adjusted(8,4,0,0),Qt::AlignTop|Qt::AlignLeft,QStringLiteral("Waterfall"));
    p.drawText(xy.adjusted(8,4,0,0),Qt::AlignTop|Qt::AlignLeft,QStringLiteral("XY"));
    p.drawText(xp.adjusted(0,0,-2,0),Qt::AlignRight|Qt::AlignBottom,QStringLiteral("Mark"));
    p.drawText(xp.adjusted(3,0,0,0),Qt::AlignLeft|Qt::AlignTop,QStringLiteral("Space"));
    if (!carrier_) {
        p.setPen(QColor(0,107,0));
        p.drawText(spec,Qt::AlignCenter,QStringLiteral("NO SIGNAL"));
        p.drawText(xy,Qt::AlignCenter,QStringLiteral("NO SIGNAL"));
    }
    const double barW=xp.width()-92;
    if (barW>40) {
        const double barX=xp.left()+46, barY=xy.top()+8;
        p.fillRect(QRectF(barX,barY,barW,4),QColor(0,56,0));
        p.fillRect(QRectF(barX+barW/2-0.5,barY-3,1,10),QColor(46,184,46));
        p.fillRect(QRectF(barX+barW*(0.5+soft_*0.5)-2,barY-2,4,8),QColor(217,255,217));
    }
}
