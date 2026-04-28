#ifndef NAVIGATIONRAIL_H
#define NAVIGATIONRAIL_H

#include <QIcon>
#include <QWidget>

class QButtonGroup;
class QPushButton;
class QVBoxLayout;

class NavigationRail : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationRail(QWidget *parent = nullptr);

    int currentIndex() const;
    void setCurrentIndex(int index);

signals:
    void currentIndexChanged(int index);

private:
    QPushButton *createNavItem(const QIcon &icon, const QString &objectName);
    void onItemToggled(int index, bool checked);

    QVBoxLayout *m_layout;
    QButtonGroup *m_buttonGroup;
};

#endif // NAVIGATIONRAIL_H
