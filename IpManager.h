#ifndef IPMANAGER_H
#define IPMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QThread;

class IpManager : public QObject
{
    Q_OBJECT

public:
    explicit IpManager(QObject *parent = nullptr);
    ~IpManager();

    QStringList listAdapters();
    void assignRandomIp(QString adapter);
    void setStaticIp(QString adapter, QString ip);
    void restoreDhcp(QString adapter);

signals:
    void noAvailableIp();
    void ipAssigned(QString ip);
    void dhcpRestored();
    void permissionDenied();

private:
    QObject *m_worker;
    QThread *m_workerThread;
};

#endif // IPMANAGER_H
