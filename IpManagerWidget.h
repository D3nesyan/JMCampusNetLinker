#ifndef IPMANAGERWIDGET_H
#define IPMANAGERWIDGET_H

#include <QWidget>

class IpManager;
class QLabel;
class QComboBox;
class QPushButton;
class QTableWidget;

class IpManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IpManagerWidget(QWidget *parent = nullptr);

private:
    void refreshAdapters();
    void refreshTable();
    QString currentAdapter() const;
    QString findMacAddressForAdapter(const QString &adapter) const;
    void setStatusMessage(const QString &text, const QString &color);

    IpManager *m_ipManager;
    QComboBox *m_adapterComboBox;
    QPushButton *m_refreshButton;
    QPushButton *m_assignButton;
    QPushButton *m_restoreDhcpButton;

    QPushButton *m_deleteRecordButton;
    QTableWidget *m_tableWidget;
    QLabel *m_statusLabel;
    QString m_lastAssignedIp;
    bool m_pendingDhcpRestore = false;
};

#endif // IPMANAGERWIDGET_H
