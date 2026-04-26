#include "IpManager.h"
#include "IpRecord.h"

#include <QDebug>
#include <QNetworkInterface>
#include <QProcess>
#include <QRandomGenerator>
#include <QThread>

namespace {

constexpr auto kSubnetMask = "255.255.0.0";
constexpr auto kGateway = "172.19.0.1";
constexpr auto kDns1 = "172.17.8.32";
constexpr auto kDns2 = "172.17.8.33";
constexpr auto kNetworkPrefix = "172.19";

QString netshNameArg(const QString &adapter)
{
    if (adapter.contains(QLatin1Char(' '))) {
        return QStringLiteral("name=\"%1\"").arg(adapter);
    }
    return QStringLiteral("name=%1").arg(adapter);
}

bool runNetsh(const QStringList &args, QString *outErr = nullptr)
{
    QProcess process;
    process.start(QStringLiteral("netsh"), args);
    if (!process.waitForStarted(5000)) {
        if (outErr) {
            *outErr = QStringLiteral("Failed to start netsh");
        }
        return false;
    }

    if (!process.waitForFinished(15000)) {
        process.kill();
        if (outErr) {
            *outErr = QStringLiteral("netsh timed out");
        }
        return false;
    }

    if (process.exitCode() != 0) {
        if (outErr) {
            *outErr = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        }
        return false;
    }

    return true;
}

bool isPermissionError(const QString &errOutput)
{
    return errOutput.contains(QStringLiteral("requires elevation"))
           || errOutput.contains(QStringLiteral("Access is denied"))
           || errOutput.contains(QStringLiteral("0x80070005"))
           || errOutput.contains(QStringLiteral("administrator"))
           || errOutput.contains(QStringLiteral("run as administrator"))
           || errOutput.contains(QStringLiteral("权限"))
           || errOutput.contains(QStringLiteral("管理员"));
}

// ---------------------------------------------------------------------------
// Worker object that runs blocking netsh calls on a background thread
// ---------------------------------------------------------------------------

class IpWorker : public QObject
{
    Q_OBJECT

public:
    explicit IpWorker(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public slots:
    void doAssignRandomIp(const QString &adapter)
    {
        QString ip;
        bool found = false;

        for (int attempt = 0; attempt < 200; ++attempt) {
            const int third = QRandomGenerator::global()->bounded(256);
            const int fourth = QRandomGenerator::global()->bounded(256);

            // Exclude 172.19.0.0 (network) and 172.19.0.1 (gateway)
            if (third == 0 && fourth <= 1) {
                continue;
            }

            ip = QStringLiteral("%1.%2.%3")
                     .arg(kNetworkPrefix)
                     .arg(third)
                     .arg(fourth);

            if (!IpRecord::instance().isIpInUse(ip)) {
                found = true;
                break;
            }
        }

        if (!found) {
            emit noAvailableIp();
            return;
        }

        // Set static IP address
        const QString nameArg = netshNameArg(adapter);
        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("set")
                 << QStringLiteral("address") << nameArg
                 << QStringLiteral("static") << ip << kSubnetMask << kGateway;

            QString errOutput;
            if (!runNetsh(args, &errOutput)) {
                if (isPermissionError(errOutput)) {
                    emit permissionDenied();
                } else {
                    emit noAvailableIp();
                }
                return;
            }
        }

        // Set primary DNS
        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("set")
                 << QStringLiteral("dns") << nameArg
                 << QStringLiteral("static") << kDns1;

            QString errOutput;
            if (!runNetsh(args, &errOutput)) {
                if (isPermissionError(errOutput)) {
                    emit permissionDenied();
                } else {
                    emit noAvailableIp();
                }
                return;
            }
        }

        // Add secondary DNS
        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("add")
                 << QStringLiteral("dns") << nameArg
                 << kDns2 << QStringLiteral("index=2");

            runNetsh(args); // best-effort — primary DNS is already set
        }

        emit ipAssigned(ip);
    }

    void doSetStaticIp(const QString &adapter, const QString &ip)
    {
        const QString nameArg = netshNameArg(adapter);

        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("set")
                 << QStringLiteral("address") << nameArg
                 << QStringLiteral("static") << ip << kSubnetMask << kGateway;

            QString errOutput;
            if (!runNetsh(args, &errOutput)) {
                if (isPermissionError(errOutput)) {
                    emit permissionDenied();
                }
                return;
            }
        }

        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("set")
                 << QStringLiteral("dns") << nameArg
                 << QStringLiteral("static") << kDns1;

            runNetsh(args);
        }

        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("add")
                 << QStringLiteral("dns") << nameArg
                 << kDns2 << QStringLiteral("index=2");

            runNetsh(args);
        }

        emit ipAssigned(ip);
    }

    void doRestoreDhcp(const QString &adapter)
    {
        const QString nameArg = netshNameArg(adapter);

        // Restore DHCP for IP address
        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("set")
                 << QStringLiteral("address") << nameArg
                 << QStringLiteral("source=dhcp");

            QString errOutput;
            if (!runNetsh(args, &errOutput)) {
                if (isPermissionError(errOutput)) {
                    emit permissionDenied();
                    return;
                }
            }
        }

        // Restore DHCP for DNS
        {
            QStringList args;
            args << QStringLiteral("interface") << QStringLiteral("ip") << QStringLiteral("set")
                 << QStringLiteral("dns") << nameArg
                 << QStringLiteral("source=dhcp");

            QString errOutput;
            if (!runNetsh(args, &errOutput)) {
                if (isPermissionError(errOutput)) {
                    emit permissionDenied();
                    return;
                }
            }
        }

        emit dhcpRestored();
    }

signals:
    void noAvailableIp();
    void ipAssigned(const QString &ip);
    void dhcpRestored();
    void permissionDenied();
};

} // namespace

// ---------------------------------------------------------------------------
// IpManager
// ---------------------------------------------------------------------------

IpManager::IpManager(QObject *parent)
    : QObject(parent)
    , m_worker(nullptr)
    , m_workerThread(new QThread(this))
{
    m_workerThread->start();

    m_worker = new IpWorker;
    m_worker->moveToThread(m_workerThread);

    auto *worker = static_cast<IpWorker *>(m_worker);
    connect(worker, &IpWorker::noAvailableIp,
            this, &IpManager::noAvailableIp);
    connect(worker, &IpWorker::ipAssigned,
            this, &IpManager::ipAssigned);
    connect(worker, &IpWorker::dhcpRestored,
            this, &IpManager::dhcpRestored);
    connect(worker, &IpWorker::permissionDenied,
            this, &IpManager::permissionDenied);
}

IpManager::~IpManager()
{
    m_workerThread->quit();
    m_workerThread->wait(5000);
    delete m_worker;
}

QStringList IpManager::listAdapters()
{
    QStringList adapters;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp)
            && iface.flags().testFlag(QNetworkInterface::IsRunning)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {

            const QString name = iface.humanReadableName();
            if (!name.isEmpty()) {
                adapters.append(name);
            }
        }
    }

    return adapters;
}

void IpManager::assignRandomIp(QString adapter)
{
    QMetaObject::invokeMethod(m_worker, "doAssignRandomIp",
                              Qt::QueuedConnection,
                              Q_ARG(QString, adapter));
}

void IpManager::setStaticIp(QString adapter, QString ip)
{
    QMetaObject::invokeMethod(m_worker, "doSetStaticIp",
                              Qt::QueuedConnection,
                              Q_ARG(QString, adapter),
                              Q_ARG(QString, ip));
}

void IpManager::restoreDhcp(QString adapter)
{
    QMetaObject::invokeMethod(m_worker, "doRestoreDhcp",
                              Qt::QueuedConnection,
                              Q_ARG(QString, adapter));
}

#include "IpManager.moc"
