#include "NavigationRail.h"

#include <QButtonGroup>
#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>

NavigationRail::NavigationRail(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_buttonGroup(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("navigationRail"));
    setFixedWidth(80);

    m_layout->setContentsMargins(0, 16, 0, 16);
    m_layout->setSpacing(12);
    m_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *authItem = createNavItem(QIcon(QStringLiteral(":/icons/nav_auth_icon.svg")),
                                   QStringLiteral("navItemAuth"));
    auto *settingsItem = createNavItem(QIcon(QStringLiteral(":/icons/nav_settings_icon.svg")),
                                       QStringLiteral("navItemSettings"));

    m_layout->addWidget(authItem);
    m_layout->addWidget(settingsItem);
    m_layout->addStretch();

    m_buttonGroup->setExclusive(true);
    authItem->setChecked(true);

    connect(m_buttonGroup, &QButtonGroup::idToggled,
            this, &NavigationRail::onItemToggled);
}

int NavigationRail::currentIndex() const
{
    return m_buttonGroup->checkedId();
}

void NavigationRail::setCurrentIndex(int index)
{
    auto *btn = m_buttonGroup->button(index);
    if (btn)
        btn->setChecked(true);
}

QPushButton *NavigationRail::createNavItem(const QIcon &icon,
                                            const QString &objectName)
{
    const int id = m_buttonGroup->buttons().size();

    auto *btn = new QPushButton(this);
    btn->setObjectName(objectName);
    btn->setIcon(icon);
    btn->setIconSize(QSize(32, 32));
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(64, 64);

    m_buttonGroup->addButton(btn, id);
    return btn;
}

void NavigationRail::onItemToggled(int index, bool checked)
{
    if (checked)
        emit currentIndexChanged(index);
}
