#include "mainwindow.h"

#include <QApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    const bool startMinimized = a.arguments().contains(QStringLiteral("--minimized"));
    if (!startMinimized) {
        w.show();
    }

    return a.exec();
}
