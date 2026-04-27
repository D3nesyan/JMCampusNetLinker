#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QFile>
#include <QFontDatabase>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Load bundled Maple Mono CN font
    const QDir fontsDir(QCoreApplication::applicationDirPath() + QStringLiteral("/fonts"));
    const QStringList ttfFiles = fontsDir.entryList({QStringLiteral("*.ttf")}, QDir::Files);
    for (const QString &file : ttfFiles) {
        QFontDatabase::addApplicationFont(fontsDir.filePath(file));
    }

    // Set application default font
    QFont defaultFont(QStringLiteral("Maple Mono CN"), 11);
    defaultFont.setStyleName(QStringLiteral("Regular"));
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferNoHinting);
    a.setFont(defaultFont);

    a.setWindowIcon(QIcon(QStringLiteral(":/icons/jimei_auth_icon.svg")));

    // Load Fluent 2 stylesheet
    QFile qss(QCoreApplication::applicationDirPath() + QStringLiteral("/fluent.qss"));
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&qss);
        a.setStyleSheet(in.readAll());
        qss.close();
    }

    MainWindow w;
    w.show();
    return a.exec();
}
