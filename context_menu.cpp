#include <QVBoxLayout>
#include <QPushButton>
#include <QFocusEvent>
#include "context_menu.h"

context_menu::context_menu(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);

    setAttribute(Qt::WA_DeleteOnClose);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(5, 5, 5, 5);
    layout_->setSpacing(2);

    setStyleSheet(R"(
        context_menu {
            background-color: white;
            border: 1px solid #E0E0E0;
            border-radius: 2px;
        }
        QPushButton {
            color: #333333;
            background-color: transparent;
            border: none;
            padding: 4px 20px;
            text-align: left;
            border-radius: 2px;
        }
        QPushButton:hover {
            background-color: #F0F0F0;
        }
        QPushButton:disabled {
            color: #AAAAAA;
        }
    )");
}

void context_menu::addAction(const QString &text)
{
    auto *button = new QPushButton(text, this);
    button->setCursor(Qt::PointingHandCursor);

    connect(button, &QPushButton::clicked, this, &context_menu::onActionClicked);

    layout_->addWidget(button);
    actions_[text] = button;
}

void context_menu::setActionEnabled(const QString &text, bool enabled)
{
    if (actions_.contains(text))
    {
        actions_[text]->setEnabled(enabled);
    }
}

void context_menu::exec(const QPoint &pos)
{
    move(pos);
    show();
}

void context_menu::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    close();
}

void context_menu::onActionClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (button != nullptr)
    {
        emit triggered(button->text());
    }
    close();
}
