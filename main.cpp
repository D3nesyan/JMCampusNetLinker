#include "mainwindow.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QFile>
#include <QFontDatabase>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Load bundled Maple Mono fonts
    const QDir fontsDir(QCoreApplication::applicationDirPath() + QStringLiteral("/fonts"));
    const QStringList ttfFiles = fontsDir.entryList({QStringLiteral("*.ttf")}, QDir::Files);
    for (const QString &file : ttfFiles) {
        QFontDatabase::addApplicationFont(fontsDir.filePath(file));
    }

    // Set application default font
    QFont defaultFont(QStringLiteral("Maple Mono NF CN"), 11);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setHintingPreference(QFont::PreferNoHinting);
    a.setFont(defaultFont);

    a.setWindowIcon(QIcon(QStringLiteral(":/icons/jimei_auth_icon.svg")));

    // Load Fluent stylesheet with theme variable substitution
    ThemeManager::instance().applyGlobalStyleSheet();

    MainWindow w;
    w.show();
    return a.exec();
}
