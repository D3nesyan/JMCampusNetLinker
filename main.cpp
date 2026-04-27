#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile qss(QStringLiteral("fluent2.qss"));
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&qss);
        a.setStyleSheet(in.readAll());
        qss.close();
    }

    MainWindow w;
    w.show();
    return a.exec();
}
