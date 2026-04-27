#include "ThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

ThemeManager &ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager()
    : QObject(nullptr)
{
    setThemeColor(QColor(0xf4, 0x2a, 0x10));
}

void ThemeManager::setThemeColor(const QColor &color)
{
    if (m_themeColor == color)
        return;

    m_themeColor = color;
    recalculateColors();
    emit themeColorChanged();
}

QColor ThemeManager::themeColor() const { return m_themeColor; }
QColor ThemeManager::light1() const { return m_light1; }
QColor ThemeManager::light2() const { return m_light2; }
QColor ThemeManager::light3() const { return m_light3; }
QColor ThemeManager::dark1() const { return m_dark1; }

void ThemeManager::recalculateColors()
{
    const qreal h = m_themeColor.hueF();
    const qreal s = m_themeColor.saturationF();
    const qreal v = m_themeColor.valueF();

    // light1: slightly lighter for hover
    m_light1 = QColor::fromHsvF(h, s, qMin(1.0, v + 0.08));
    // light2: noticeably lighter
    m_light2 = QColor::fromHsvF(h, s * 0.85, qMin(1.0, v + 0.18));
    // light3: very light for pressed state
    m_light3 = QColor::fromHsvF(h, s * 0.65, qMin(1.0, v + 0.30));
    // dark1: darker for bottom border / press
    m_dark1 = QColor::fromHsvF(h, s, qMax(0.0, v - 0.15));
}

QHash<QString, QString> ThemeManager::variableMap() const
{
    return {
        {QStringLiteral("ThemeColorPrimary"), m_themeColor.name()},
        {QStringLiteral("ThemeColorLight1"),  m_light1.name()},
        {QStringLiteral("ThemeColorLight2"),  m_light2.name()},
        {QStringLiteral("ThemeColorLight3"),  m_light3.name()},
        {QStringLiteral("ThemeColorDark1"),   m_dark1.name()},
    };
}

QString ThemeManager::processVariables(const QString &qss) const
{
    QString result = qss;
    const auto vars = variableMap();
    for (auto it = vars.begin(); it != vars.end(); ++it) {
        result.replace(QStringLiteral("{{%1}}").arg(it.key()), it.value());
    }
    return result;
}

QString ThemeManager::processedStyleSheet(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return {};

    QTextStream in(&file);
    return processVariables(in.readAll());
}

void ThemeManager::applyGlobalStyleSheet()
{
    const QString qssPath = QCoreApplication::applicationDirPath()
                            + QStringLiteral("/fluent.qss");
    const QString qss = processedStyleSheet(qssPath);
    if (!qss.isEmpty())
        qApp->setStyleSheet(qss);
}
