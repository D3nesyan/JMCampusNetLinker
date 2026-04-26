#include "IpRecord.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QVariant>

namespace {

constexpr auto kConnectionName = "IpRecordConnection";

} // namespace

IpRecord &IpRecord::instance()
{
    static IpRecord instance;
    return instance;
}

IpRecord::IpRecord()
    : m_db(new QSqlDatabase)
    , m_initialized(false)
{
    ensureInitialized();
}

IpRecord::~IpRecord()
{
    if (m_db && m_db->isValid()) {
        m_db->close();
    }
    delete m_db;
}

bool IpRecord::addRecord(QString mac, QString adapter, QString ip)
{
    if (!ensureInitialized()) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
            "INSERT INTO ip_records "
            "(mac_address, adapter_name, assigned_ip, hostname, assigned_at, is_active) "
            "VALUES (:mac, :adapter, :ip, :hostname, :assignedAt, 1)"));
    query.bindValue(QStringLiteral(":mac"), mac);
    query.bindValue(QStringLiteral(":adapter"), adapter);
    query.bindValue(QStringLiteral(":ip"), ip);
    query.bindValue(QStringLiteral(":hostname"), QSysInfo::machineHostName());
    query.bindValue(QStringLiteral(":assignedAt"),
                    QDateTime::currentDateTime().toString(Qt::ISODate));

    return query.exec();
}

bool IpRecord::deactivate(QString ip)
{
    if (!ensureInitialized()) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
            "UPDATE ip_records SET is_active = 0 WHERE assigned_ip = :ip AND is_active = 1"));
    query.bindValue(QStringLiteral(":ip"), ip);

    return query.exec();
}

bool IpRecord::isIpInUse(QString ip)
{
    if (!ensureInitialized()) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
            "SELECT COUNT(1) FROM ip_records WHERE assigned_ip = :ip AND is_active = 1"));
    query.bindValue(QStringLiteral(":ip"), ip);

    if (!query.exec() || !query.next()) {
        return false;
    }

    return query.value(0).toInt() > 0;
}

QList<IpRecord::Record> IpRecord::getActiveRecords()
{
    QList<Record> records;
    if (!ensureInitialized()) {
        return records;
    }

    QSqlQuery query(*m_db);
    if (!query.exec(QStringLiteral(
                "SELECT id, mac_address, adapter_name, assigned_ip, hostname, assigned_at, is_active "
                "FROM ip_records WHERE is_active = 1 ORDER BY assigned_at DESC"))) {
        return records;
    }

    while (query.next()) {
        Record record;
        record.id = query.value(0).toInt();
        record.macAddress = query.value(1).toString();
        record.adapterName = query.value(2).toString();
        record.assignedIp = query.value(3).toString();
        record.hostname = query.value(4).toString();
        record.assignedAt = query.value(5).toString();
        record.isActive = query.value(6).toInt() == 1;
        records.append(record);
    }

    return records;
}

bool IpRecord::exportCsv(QString filePath)
{
    if (!ensureInitialized()) {
        return false;
    }

    QSqlQuery query(*m_db);
    if (!query.exec(QStringLiteral(
                "SELECT id, mac_address, adapter_name, assigned_ip, hostname, assigned_at, is_active "
                "FROM ip_records ORDER BY assigned_at DESC"))) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out << "id,mac_address,adapter_name,assigned_ip,hostname,assigned_at,is_active\n";

    while (query.next()) {
        out << query.value(0).toString() << ','
            << escapeCsv(query.value(1).toString()) << ','
            << escapeCsv(query.value(2).toString()) << ','
            << escapeCsv(query.value(3).toString()) << ','
            << escapeCsv(query.value(4).toString()) << ','
            << escapeCsv(query.value(5).toString()) << ','
            << query.value(6).toString() << '\n';
    }

    return out.status() == QTextStream::Ok;
}

bool IpRecord::ensureInitialized()
{
    if (m_initialized && m_db->isOpen()) {
        return true;
    }

    const QString connectionName = QString::fromLatin1(kConnectionName);

    if (!QSqlDatabase::contains(connectionName)) {
        *m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                          connectionName);
    } else {
        *m_db = QSqlDatabase::database(connectionName);
    }

    const QString dbPath = databaseFilePath();
    QFileInfo info(dbPath);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    m_db->setDatabaseName(dbPath);
    if (!m_db->isOpen() && !m_db->open()) {
        return false;
    }

    QSqlQuery query(*m_db);
    const bool ok = query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS ip_records ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "mac_address TEXT, "
            "adapter_name TEXT, "
            "assigned_ip TEXT, "
            "hostname TEXT, "
            "assigned_at TEXT, "
            "is_active INTEGER"
            ")"));

    m_initialized = ok;
    return ok;
}

QString IpRecord::databaseFilePath() const
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QDir::currentPath();
    }

    return basePath + QStringLiteral("/campusnet/records.db");
}

QString IpRecord::escapeCsv(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}
