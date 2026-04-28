#include "ThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>

ThemeManager &ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager()
    : QObject(nullptr)
{
    setThemeColor(QColor("#D23B2C"));
}

void ThemeManager::setThemeColor(const QColor &seed)
{
    if (m_seed == seed)
        return;

    m_seed = seed;
    recalculateColors();
    emit themeColorChanged();
}

QColor ThemeManager::themeColor() const { return m_seed; }

QColor ThemeManager::primary() const { return m_primary; }
QColor ThemeManager::onPrimary() const { return m_onPrimary; }
QColor ThemeManager::primaryContainer() const { return m_primaryContainer; }
QColor ThemeManager::onPrimaryContainer() const { return m_onPrimaryContainer; }
QColor ThemeManager::secondary() const { return m_secondary; }
QColor ThemeManager::secondaryContainer() const { return m_secondaryContainer; }
QColor ThemeManager::onSecondaryContainer() const { return m_onSecondaryContainer; }
QColor ThemeManager::tertiary() const { return m_tertiary; }
QColor ThemeManager::tertiaryContainer() const { return m_tertiaryContainer; }
QColor ThemeManager::onTertiaryContainer() const { return m_onTertiaryContainer; }
QColor ThemeManager::error() const { return m_error; }
QColor ThemeManager::onError() const { return m_onError; }
QColor ThemeManager::errorContainer() const { return m_errorContainer; }
QColor ThemeManager::onErrorContainer() const { return m_onErrorContainer; }
QColor ThemeManager::surface() const { return m_surface; }
QColor ThemeManager::onSurface() const { return m_onSurface; }
QColor ThemeManager::surfaceVariant() const { return m_surfaceVariant; }
QColor ThemeManager::onSurfaceVariant() const { return m_onSurfaceVariant; }
QColor ThemeManager::outline() const { return m_outline; }
QColor ThemeManager::outlineVariant() const { return m_outlineVariant; }
QColor ThemeManager::primaryHover() const { return m_primaryHover; }
QColor ThemeManager::primaryPress() const { return m_primaryPress; }
QColor ThemeManager::primaryContainerHover() const { return m_primaryContainerHover; }
QColor ThemeManager::primaryContainerPress() const { return m_primaryContainerPress; }
QColor ThemeManager::surfaceContainerLow() const { return m_surfaceContainerLow; }
QColor ThemeManager::surfaceContainerHighest() const { return m_surfaceContainerHighest; }

QColor ThemeManager::blend(const QColor &base, const QColor &overlay) const
{
    const qreal alpha = overlay.alphaF();
    return QColor(
        qRound(base.red()   * (1.0 - alpha) + overlay.red()   * alpha),
        qRound(base.green() * (1.0 - alpha) + overlay.green() * alpha),
        qRound(base.blue()  * (1.0 - alpha) + overlay.blue()  * alpha)
    );
}

QColor ThemeManager::toneColor(qreal hue, qreal chroma, int tone) const
{
    const qreal lightness = tone / 100.0;
    qreal saturation = chroma;

    // Chroma peaks at tone ~40, falls off toward extremes
    if (tone <= 40)
        saturation = chroma * (0.4 + 0.6 * tone / 40.0);
    else
        saturation = chroma * (1.0 - 0.6 * (tone - 40.0) / 60.0);

    saturation = qBound(0.0, saturation, 1.0);

    return QColor::fromHslF(hue, saturation, lightness, 1.0);
}

QColor ThemeManager::surfaceColor(qreal hue, qreal chroma, int tone) const
{
    const qreal lightness = tone / 100.0;
    const qreal saturation = chroma * 0.03;
    return QColor::fromHslF(hue, qBound(0.0, saturation, 1.0), lightness, 1.0);
}

void ThemeManager::recalculateColors()
{
    const qreal hue = m_seed.hueF() >= 0.0 ? m_seed.hueF() : 0.0;
    const qreal chroma = m_seed.saturationF();
    const qreal hueSecondary = std::fmod(hue + 0.0167, 1.0);  // ~6° shift
    const qreal hueTertiary = std::fmod(hue + 0.1667, 1.0);   // ~60° shift
    const qreal hueError = 0.0111;  // ~4° — M3 error red

    // Primary family
    m_primary            = toneColor(hue, chroma, 40);
    m_onPrimary          = QColor(Qt::white);
    m_primaryContainer   = toneColor(hue, chroma, 90);
    m_onPrimaryContainer = toneColor(hue, chroma, 10);

    // Secondary family
    m_secondary            = toneColor(hueSecondary, chroma * 0.5, 40);
    m_secondaryContainer   = toneColor(hueSecondary, chroma * 0.25, 90);
    m_onSecondaryContainer = toneColor(hueSecondary, chroma * 0.55, 10);

    // Tertiary family
    m_tertiary            = toneColor(hueTertiary, chroma * 0.5, 40);
    m_tertiaryContainer   = toneColor(hueTertiary, chroma * 0.25, 90);
    m_onTertiaryContainer = toneColor(hueTertiary, chroma * 0.55, 10);

    // Error family (fixed hue, tonal palette from M3 spec)
    m_error            = toneColor(hueError, 0.65, 40);
    m_onError          = QColor(Qt::white);
    m_errorContainer   = toneColor(hueError, 0.15, 90);
    m_onErrorContainer = toneColor(hueError, 0.50, 10);

    // Surface / neutral
    m_surface            = surfaceColor(hue, chroma, 98);
    m_onSurface          = toneColor(hue, 0.12, 10);
    m_surfaceVariant     = surfaceColor(hue, chroma, 95);
    m_onSurfaceVariant   = toneColor(hue, 0.20, 30);
    m_outline            = toneColor(hue, 0.08, 50);
    m_outlineVariant     = toneColor(hue, 0.06, 80);

    // Surface containers (elevated surfaces)
    m_surfaceContainerLow     = surfaceColor(hue, chroma, 96);
    m_surfaceContainerHighest = surfaceColor(hue, chroma, 94);

    // State layers (pre-blended for filled buttons)
    m_primaryHover            = blend(m_primary, QColor(255, 255, 255, 20));
    m_primaryPress            = blend(m_primary, QColor(255, 255, 255, 30));
    m_primaryContainerHover   = blend(m_primaryContainer, QColor(0, 0, 0, 15));
    m_primaryContainerPress   = blend(m_primaryContainer, QColor(0, 0, 0, 25));
}

QHash<QString, QString> ThemeManager::variableMap() const
{
    return {
        // Primary
        {QStringLiteral("M3Primary"),             m_primary.name()},
        {QStringLiteral("M3OnPrimary"),           m_onPrimary.name()},
        {QStringLiteral("M3PrimaryContainer"),    m_primaryContainer.name()},
        {QStringLiteral("M3OnPrimaryContainer"),  m_onPrimaryContainer.name()},
        // Secondary
        {QStringLiteral("M3Secondary"),           m_secondary.name()},
        {QStringLiteral("M3SecondaryContainer"),  m_secondaryContainer.name()},
        {QStringLiteral("M3OnSecondaryContainer"),m_onSecondaryContainer.name()},
        // Tertiary
        {QStringLiteral("M3Tertiary"),            m_tertiary.name()},
        {QStringLiteral("M3TertiaryContainer"),   m_tertiaryContainer.name()},
        {QStringLiteral("M3OnTertiaryContainer"), m_onTertiaryContainer.name()},
        // Error
        {QStringLiteral("M3Error"),               m_error.name()},
        {QStringLiteral("M3OnError"),             m_onError.name()},
        {QStringLiteral("M3ErrorContainer"),      m_errorContainer.name()},
        {QStringLiteral("M3OnErrorContainer"),    m_onErrorContainer.name()},
        // Surface
        {QStringLiteral("M3Surface"),             m_surface.name()},
        {QStringLiteral("M3OnSurface"),           m_onSurface.name()},
        {QStringLiteral("M3SurfaceVariant"),      m_surfaceVariant.name()},
        {QStringLiteral("M3OnSurfaceVariant"),    m_onSurfaceVariant.name()},
        {QStringLiteral("M3Outline"),             m_outline.name()},
        {QStringLiteral("M3OutlineVariant"),      m_outlineVariant.name()},
        // State layers
        {QStringLiteral("M3PrimaryHover"),        m_primaryHover.name()},
        {QStringLiteral("M3PrimaryPress"),        m_primaryPress.name()},
        {QStringLiteral("M3PrimaryContainerHover"), m_primaryContainerHover.name()},
        {QStringLiteral("M3PrimaryContainerPress"), m_primaryContainerPress.name()},
        // Surface containers
        {QStringLiteral("M3SurfaceContainerLow"),     m_surfaceContainerLow.name()},
        {QStringLiteral("M3SurfaceContainerHighest"), m_surfaceContainerHighest.name()},
        // RGB components for rgba() state layers
        {QStringLiteral("M3PrimaryR"), QString::number(m_primary.red())},
        {QStringLiteral("M3PrimaryG"), QString::number(m_primary.green())},
        {QStringLiteral("M3PrimaryB"), QString::number(m_primary.blue())},
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
                            + QStringLiteral("/theme.qss");
    const QString qss = processedStyleSheet(qssPath);
    if (!qss.isEmpty())
        qApp->setStyleSheet(qss);
}
