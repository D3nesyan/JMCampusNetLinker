#include "IpManager.h"
#include "IpRecord.h"

#include <QDebug>
#include <QNetworkInterface>
#include <QProcess>
#include <QRandomGenerator>
#include <QThread>

namespace {

constexpr auto kGateway = "172.19.0.1";
constexpr auto kDns1 = "172.17.8.32";
constexpr auto kDns2 = "172.17.8.33";
constexpr auto kNetworkPrefix = "172.19";

QString psEscape(const QString &str)
{
    QString escaped = str;
    escaped.replace('\'', QStringLiteral("''"));
    return escaped;
}

bool runPowerShell(const QString &script, QString *outErr = nullptr)
{
    QProcess process;
    process.start(QStringLiteral("powershell"), {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-Command"),
        script
    });
    if (!process.waitForStarted(5000)) {
        if (outErr) {
            *outErr = QStringLiteral("Failed to start PowerShell");
        }
        return false;
    }

    if (!process.waitForFinished(30000)) {
        process.kill();
        if (outErr) {
            *outErr = QStringLiteral("PowerShell timed out");
        }
        return false;
    }

    if (process.exitCode() != 0) {
        if (outErr) {
            const QString stdErr = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *outErr = stdErr.isEmpty()
                    ? QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed()
                    : stdErr;
        }
        return false;
    }

    return true;
}

bool isPermissionError(const QString &errOutput)
{
    return errOutput.contains(QStringLiteral("Access is denied"))
           || errOutput.contains(QStringLiteral("Access denied"))
           || errOutput.contains(QStringLiteral("拒绝访问"))
           || errOutput.contains(QStringLiteral("requires elevation"))
           || errOutput.contains(QStringLiteral("Run as administrator"))
           || errOutput.contains(QStringLiteral("administrator"))
           || errOutput.contains(QStringLiteral("0x80070005"))
           || errOutput.contains(QStringLiteral("UnauthorizedAccessException"))
           || errOutput.contains(QStringLiteral("Permission denied"))
           || errOutput.contains(QStringLiteral("权限"))
           || errOutput.contains(QStringLiteral("管理员"));
}

QString setStaticIpScript(const QString &adapter, const QString &ip)
{
    const QString alias = psEscape(adapter);
    return QStringLiteral(
        "$ErrorActionPreference = 'Stop'; "
        "try { "
        "  Get-NetIPAddress -InterfaceAlias '%1' -AddressFamily IPv4 -ErrorAction SilentlyContinue | "
        "    Where-Object { $_.PrefixOrigin -ne 'Dhcp' } | "
        "    Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue; "
        "  Get-NetRoute -InterfaceAlias '%1' -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue | "
        "    Remove-NetRoute -Confirm:$false -ErrorAction SilentlyContinue; "
        "  New-NetIPAddress -InterfaceAlias '%1' -AddressFamily IPv4 -IPAddress '%2' -PrefixLength 16 -DefaultGateway %3; "
        "  Set-DnsClientServerAddress -InterfaceAlias '%1' -ServerAddresses ('%4', '%5'); "
        "} catch { "
        "  Write-Error $_.Exception.Message; "
        "  exit 1; "
        "}")
        .arg(alias, ip, QString::fromLatin1(kGateway),
             QString::fromLatin1(kDns1), QString::fromLatin1(kDns2));
}

QString restoreDhcpScript(const QString &adapter)
{
    const QString alias = psEscape(adapter);
    return QStringLiteral(
        "$ErrorActionPreference = 'Stop'; "
        "try { "
        "  Set-NetIPInterface -InterfaceAlias '%1' -AddressFamily IPv4 -Dhcp Enabled; "
        "  Set-DnsClientServerAddress -InterfaceAlias '%1' -ResetServerAddresses; "
        "} catch { "
        "  Write-Error $_.Exception.Message; "
        "  exit 1; "
        "}").arg(alias);
}

// ---------------------------------------------------------------------------
// Worker object that runs blocking PowerShell calls on a background thread
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

        const QString script = setStaticIpScript(adapter, ip);
        QString errOutput;
        if (!runPowerShell(script, &errOutput)) {
            if (isPermissionError(errOutput)) {
                emit permissionDenied();
            } else {
                emit operationFailed(errOutput);
            }
            return;
        }

        emit ipAssigned(ip);
    }

    void doSetStaticIp(const QString &adapter, const QString &ip)
    {
        const QString script = setStaticIpScript(adapter, ip);
        QString errOutput;
        if (!runPowerShell(script, &errOutput)) {
            if (isPermissionError(errOutput)) {
                emit permissionDenied();
            } else {
                emit operationFailed(errOutput);
            }
            return;
        }

        emit ipAssigned(ip);
    }

    void doRestoreDhcp(const QString &adapter)
    {
        const QString script = restoreDhcpScript(adapter);
        QString errOutput;
        if (!runPowerShell(script, &errOutput)) {
            if (isPermissionError(errOutput)) {
                emit permissionDenied();
            } else {
                emit operationFailed(errOutput);
            }
            return;
        }

        emit dhcpRestored();
    }

signals:
    void noAvailableIp();
    void ipAssigned(const QString &ip);
    void dhcpRestored();
    void permissionDenied();
    void operationFailed(const QString &reason);
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
    connect(worker, &IpWorker::operationFailed,
            this, &IpManager::operationFailed);
}

IpManager::~IpManager()
{
    m_workerThread->quit();
    m_workerThread->wait(5000);
    delete m_worker;
}

// Virtual adapter name patterns to exclude (case-insensitive)
static bool isVirtualAdapter(const QString &name)
{
    static const QStringList virtualPatterns = {
        QStringLiteral("ZeroTier"),
        QStringLiteral("VMware"),
        QStringLiteral("VirtualBox"),
        QStringLiteral("vEthernet"),
        QStringLiteral("Hyper-V"),
        QStringLiteral("WSL"),
        QStringLiteral("Bluetooth"),
        QStringLiteral("TAP"),
        QStringLiteral("VPN"),
        QStringLiteral("Tunnel"),
        QStringLiteral("Tailscale"),
        QStringLiteral("WireGuard"),
        QStringLiteral("Radmin"),
        QStringLiteral("Hamachi"),
        QStringLiteral("Loopback"),
    };

    const QString lower = name.toLower();
    for (const QString &pattern : virtualPatterns) {
        if (lower.contains(pattern.toLower())) {
            return true;
        }
    }
    return false;
}

QStringList IpManager::listAdapters()
{
    QStringList adapters;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {

            const QString name = iface.humanReadableName();
            if (!name.isEmpty() && !isVirtualAdapter(name)) {
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
