#ifndef IPRECORD_H
#define IPRECORD_H

#include <QList>
#include <QString>

class QSqlDatabase;

class IpRecord
{
public:
    struct Record
    {
        int id = 0;
        QString macAddress;
        QString adapterName;
        QString assignedIp;
        QString hostname;
        QString assignedAt;
        bool isActive = false;
    };

    static IpRecord &instance();

    bool addRecord(QString mac, QString adapter, QString ip);
    bool deactivate(QString ip);
    bool deactivateByAdapter(QString adapter);
    bool isIpInUse(QString ip);
    QList<Record> getActiveRecords();

private:
    IpRecord();
    ~IpRecord();

    IpRecord(const IpRecord &) = delete;
    IpRecord &operator=(const IpRecord &) = delete;

    bool ensureInitialized();
    QString databaseFilePath() const;


    QSqlDatabase *m_db;
    bool m_initialized;
};

#endif // IPRECORD_H
