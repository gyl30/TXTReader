#include "splitter.h"
#include <QPainter>

AnimatedSplitterHandle::AnimatedSplitterHandle(Qt::Orientation orientation, QSplitter* parent) : QSplitterHandle(orientation, parent), offset_(0)
{
    timer_ = new QTimer(this);
    connect(timer_,
            &QTimer::timeout,
            this,
            [this]()
            {
                offset_ += 2;
                if (offset_ > height())
                {
                    offset_ = 0;
                }
                update();
            });
    timer_->start(300);
}

void AnimatedSplitterHandle::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    QRect r = rect();
    QLinearGradient gradient(0, -offset_, 0, r.height() - offset_);
    gradient.setColorAt(0.0, QColor(255, 128, 128, 150));
    gradient.setColorAt(0.14, QColor(255, 200, 128, 150));
    gradient.setColorAt(0.28, QColor(255, 255, 128, 150));
    gradient.setColorAt(0.42, QColor(128, 255, 128, 150));
    gradient.setColorAt(0.57, QColor(128, 200, 255, 150));
    gradient.setColorAt(0.71, QColor(180, 128, 255, 150));
    gradient.setColorAt(0.85, QColor(220, 128, 255, 150));
    gradient.setColorAt(1.0, QColor(255, 128, 128, 150));
    painter.fillRect(r, gradient);
}
