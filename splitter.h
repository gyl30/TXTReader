#ifndef SPLITTER_H
#define SPLITTER_H

#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>

class AnimatedSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
   public:
    AnimatedSplitterHandle(Qt::Orientation orientation, QSplitter* parent);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QTimer* timer_;
    int offset_;
};

class AnimatedSplitter : public QSplitter
{
    Q_OBJECT
   public:
    explicit AnimatedSplitter(Qt::Orientation orientation, QWidget* parent = nullptr) : QSplitter(orientation, parent) {}

   protected:
    QSplitterHandle* createHandle() override { return new AnimatedSplitterHandle(orientation(), this); }
};
#endif
