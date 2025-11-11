#ifndef SPLITTER_H
#define SPLITTER_H

#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>

class animated_splitter_handle : public QSplitterHandle
{
    Q_OBJECT
   public:
    animated_splitter_handle(Qt::Orientation orientation, QSplitter* parent);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QTimer* timer_;
    int offset_;
};

class animated_splitter : public QSplitter
{
    Q_OBJECT
   public:
    explicit animated_splitter(Qt::Orientation orientation, QWidget* parent = nullptr) : QSplitter(orientation, parent) {}

   protected:
    QSplitterHandle* createHandle() override { return new animated_splitter_handle(orientation(), this); }
};
#endif
