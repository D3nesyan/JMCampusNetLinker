#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager &instance();

    void setThemeColor(const QColor &color);

    QColor themeColor() const;
    QColor light1() const;
    QColor light2() const;
    QColor light3() const;
    QColor dark1() const;

    QString processedStyleSheet(const QString &filePath) const;
    QString processVariables(const QString &qss) const;
    void applyGlobalStyleSheet();

signals:
    void themeColorChanged();

private:
    ThemeManager();

    QHash<QString, QString> variableMap() const;

    QColor m_themeColor;
    QColor m_light1;
    QColor m_light2;
    QColor m_light3;
    QColor m_dark1;

    void recalculateColors();
};

#endif // THEMEMANAGER_H
