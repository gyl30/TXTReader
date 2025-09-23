#include <QApplication>
#include <string>
#include "log.h"
#include "main_window.h"
#include "scoped_exit.h"

int main(int argc, char *argv[])
{
    std::string app_name(argv[0]);
    init_log(app_name + ".log");
    DEFER(shutdown_log());
    QApplication a(argc, argv);
    main_window w;
    w.show();
    return QApplication::exec();
}
