#ifndef TXTREADER_CONTEXT_MENU_H
#define TXTREADER_CONTEXT_MENU_H

#include <QWidget>
#include <QMap>

class QVBoxLayout;
class QPushButton;

class context_menu : public QWidget
{
    Q_OBJECT

   public:
    explicit context_menu(QWidget *parent = nullptr);
    void addAction(const QString &text);
    void setActionEnabled(const QString &text, bool enabled);
    void exec(const QPoint &pos);

   signals:
    void triggered(const QString &actionText);

   protected:
    void focusOutEvent(QFocusEvent *event) override;

   private slots:
    void onActionClicked();

   private:
    QVBoxLayout *layout_;
    QMap<QString, QPushButton *> actions_;
};

#endif
