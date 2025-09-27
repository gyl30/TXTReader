#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QIcon>
#include <QString>
#include <string>
#include "log.h"
#include "main_window.h"
#include "scoped_exit.h"

static QIcon emoji_to_icon(const QString &emoji, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    auto font = QApplication::font();
    font.setPointSizeF(size * 0.6);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, emoji);
    painter.end();
    return pixmap;
}

int main(int argc, char *argv[])
{
    std::string app_name(argv[0]);
    init_log(app_name + ".log");
    DEFER(shutdown_log());
    QApplication a(argc, argv);
    QFont font;
    QStringList families;
    families << "Microsoft YaHei UI"
             << "Noto Sans CJK SC"
             << "PingFang SC"
             << "sans-serif";
    font.setFamilies(families);
    QApplication::setFont(font);
    main_window w;
    QApplication::setWindowIcon(emoji_to_icon("📖", 64));
    w.show();
    return QApplication::exec();
}
