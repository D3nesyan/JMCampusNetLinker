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

    void setThemeColor(const QColor &seed);

    QColor themeColor() const;

    QColor primary() const;
    QColor onPrimary() const;
    QColor primaryContainer() const;
    QColor onPrimaryContainer() const;
    QColor secondary() const;
    QColor secondaryContainer() const;
    QColor onSecondaryContainer() const;
    QColor tertiary() const;
    QColor tertiaryContainer() const;
    QColor onTertiaryContainer() const;
    QColor error() const;
    QColor onError() const;
    QColor errorContainer() const;
    QColor onErrorContainer() const;
    QColor surface() const;
    QColor onSurface() const;
    QColor surfaceVariant() const;
    QColor onSurfaceVariant() const;
    QColor outline() const;
    QColor outlineVariant() const;
    QColor primaryHover() const;
    QColor primaryPress() const;
    QColor primaryContainerHover() const;
    QColor primaryContainerPress() const;
    QColor surfaceContainerLow() const;
    QColor surfaceContainerHighest() const;

    QString processedStyleSheet(const QString &filePath) const;
    QString processVariables(const QString &qss) const;
    void applyGlobalStyleSheet();

signals:
    void themeColorChanged();

private:
    ThemeManager();

    QHash<QString, QString> variableMap() const;

    QColor toneColor(qreal hue, qreal chroma, int tone) const;
    QColor surfaceColor(qreal hue, qreal chroma, int tone) const;
    QColor blend(const QColor &base, const QColor &overlay) const;

    QColor m_seed;

    QColor m_primary;
    QColor m_onPrimary;
    QColor m_primaryContainer;
    QColor m_onPrimaryContainer;
    QColor m_secondary;
    QColor m_secondaryContainer;
    QColor m_onSecondaryContainer;
    QColor m_tertiary;
    QColor m_tertiaryContainer;
    QColor m_onTertiaryContainer;
    QColor m_error;
    QColor m_onError;
    QColor m_errorContainer;
    QColor m_onErrorContainer;
    QColor m_surface;
    QColor m_onSurface;
    QColor m_surfaceVariant;
    QColor m_onSurfaceVariant;
    QColor m_outline;
    QColor m_outlineVariant;
    QColor m_primaryHover;
    QColor m_primaryPress;
    QColor m_primaryContainerHover;
    QColor m_primaryContainerPress;
    QColor m_surfaceContainerLow;
    QColor m_surfaceContainerHighest;

    void recalculateColors();
};

#endif // THEMEMANAGER_H
