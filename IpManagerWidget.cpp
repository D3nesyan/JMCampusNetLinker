#include "IpManagerWidget.h"

#include "IpManager.h"
#include "IpRecord.h"

#include <QDebug>
#include <QAbstractItemView>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QNetworkInterface>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr auto kSuccessColor = "#107C41";
constexpr auto kErrorColor = "#C42B1C";
constexpr auto kWarningColor = "#797775";

QTableWidgetItem *makeReadOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

IpManagerWidget::IpManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_ipManager(new IpManager(this))
    , m_adapterComboBox(new QComboBox(this))
    , m_refreshButton(new QPushButton(QStringLiteral("刷新"), this))
    , m_assignButton(new QPushButton(QStringLiteral("随机分配 IP"), this))
    , m_restoreDhcpButton(new QPushButton(QStringLiteral("还原 DHCP"), this))
    , m_exportButton(new QPushButton(QStringLiteral("导出 CSV"), this))
    , m_deleteRecordButton(new QPushButton(QStringLiteral("删除记录"), this))
    , m_tableWidget(new QTableWidget(this))
    , m_statusLabel(new QLabel(this))
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *configGroupBox = new QGroupBox(QStringLiteral("网段配置"), this);
    auto *configLayout = new QFormLayout(configGroupBox);

    auto *adapterRowLayout = new QHBoxLayout;
    adapterRowLayout->addWidget(m_adapterComboBox, 1);
    adapterRowLayout->addWidget(m_refreshButton);

    configLayout->addRow(QStringLiteral("网卡选择"), adapterRowLayout);

    auto *hintLabel = new QLabel(
        QStringLiteral("请选择连接校园网的网卡，选错可能导致各种异常。\n"
                       "随机分配 IP 可绕过夜间断网（可能不可用，可多试几次）。\n"
                       "切换其他网络前务必先点击「还原 DHCP」恢复。"),
        configGroupBox);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #616161; font-size: 12px; padding-left: 4px; }"));
    configLayout->addRow(hintLabel);

    configLayout->addRow(QStringLiteral("网段"),
                         new QLabel(QStringLiteral("172.19.0.0 / 16"), configGroupBox));
    configLayout->addRow(QStringLiteral("子网掩码"),
                         new QLabel(QStringLiteral("255.255.0.0"), configGroupBox));
    configLayout->addRow(QStringLiteral("默认网关"),
                         new QLabel(QStringLiteral("172.19.0.1"), configGroupBox));
    configLayout->addRow(QStringLiteral("DNS"),
                         new QLabel(QStringLiteral("172.17.8.32 / 172.17.8.33"), configGroupBox));

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_assignButton);
    buttonLayout->addWidget(m_restoreDhcpButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_deleteRecordButton);
    buttonLayout->addStretch();

    m_tableWidget->setColumnCount(5);
    m_tableWidget->setHorizontalHeaderLabels(QStringList{
            QStringLiteral("MAC地址"),
            QStringLiteral("网卡"),
            QStringLiteral("分配IP"),
            QStringLiteral("主机名"),
            QStringLiteral("分配时间")
    });
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);

    mainLayout->addWidget(configGroupBox);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_tableWidget, 1);
    mainLayout->addWidget(m_statusLabel);

    connect(m_refreshButton, &QPushButton::clicked, this, &IpManagerWidget::refreshAdapters);

    connect(m_assignButton, &QPushButton::clicked, this, [this] {
        const QString adapter = currentAdapter();
        if (adapter.isEmpty()) {
            setStatusMessage(QStringLiteral("请先选择网卡"), kWarningColor);
            return;
        }

        m_ipManager->assignRandomIp(adapter);
    });

    connect(m_restoreDhcpButton, &QPushButton::clicked, this, [this] {
        const QString adapter = currentAdapter();
        if (adapter.isEmpty()) {
            setStatusMessage(QStringLiteral("请先选择网卡"), kWarningColor);
            return;
        }

        m_pendingDhcpRestore = true;
        m_ipManager->restoreDhcp(adapter);
    });

    connect(m_exportButton, &QPushButton::clicked, this, [this] {
        const QString filePath = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("导出 CSV"),
                QStringLiteral("ip_records.csv"),
                QStringLiteral("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (IpRecord::instance().exportCsv(filePath)) {
            setStatusMessage(QStringLiteral("CSV 导出成功"), kSuccessColor);
        } else {
            setStatusMessage(QStringLiteral("CSV 导出失败"), kErrorColor);
        }
    });

    connect(m_deleteRecordButton, &QPushButton::clicked, this, [this] {
        const int row = m_tableWidget->currentRow();
        if (row < 0) {
            setStatusMessage(QStringLiteral("请先选择要删除的记录"), kWarningColor);
            return;
        }

        auto *item = m_tableWidget->item(row, 2); // 分配IP column
        if (!item) {
            return;
        }

        const QString ip = item->text();
        IpRecord::instance().deactivate(ip);
        m_lastAssignedIp.clear();
        refreshTable();
        setStatusMessage(QStringLiteral("已删除记录 %1").arg(ip), kSuccessColor);
    });

    connect(m_ipManager, &IpManager::ipAssigned, this, [this](const QString &ip) {
        const QString adapter = currentAdapter();
        const QString mac = findMacAddressForAdapter(adapter);

        m_lastAssignedIp = ip;
        IpRecord::instance().addRecord(mac, adapter, ip);
        refreshTable();
        setStatusMessage(QStringLiteral("已分配 %1").arg(ip), kSuccessColor);
    });

    connect(m_ipManager, &IpManager::dhcpRestored, this, [this] {
        m_pendingDhcpRestore = false;
        const QString adapter = currentAdapter();
        IpRecord::instance().deactivateByAdapter(adapter);
        m_lastAssignedIp.clear();
        refreshTable();
        setStatusMessage(QStringLiteral("已还原 DHCP"), kSuccessColor);
    });

    connect(m_ipManager, &IpManager::permissionDenied, this, [this] {
        setStatusMessage(QStringLiteral("请以管理员身份运行本程序"),
                         kErrorColor);
    });

    connect(m_ipManager, &IpManager::noAvailableIp, this, [this] {
        setStatusMessage(QStringLiteral("172.19 网段未找到可用 IP，请检查网络连通性"),
                         kWarningColor);
    });

    connect(m_ipManager, &IpManager::operationFailed, this,
            [this](const QString &reason) {
                if (m_pendingDhcpRestore) {
                    m_pendingDhcpRestore = false;
                    const QString adapter = currentAdapter();
                    IpRecord::instance().deactivateByAdapter(adapter);
                    m_lastAssignedIp.clear();
                    refreshTable();
                    setStatusMessage(
                        QStringLiteral("DHCP 还原失败，记录已清除 (%1)").arg(reason),
                        kWarningColor);
                } else {
                    setStatusMessage(QStringLiteral("操作失败: %1").arg(reason), kErrorColor);
                }
            });

    refreshAdapters();
    refreshTable();
    setStatusMessage(QStringLiteral("就绪"), QStringLiteral("#616161"));
}

void IpManagerWidget::refreshAdapters()
{
    const QString previous = currentAdapter();
    QStringList adapters = m_ipManager->listAdapters();

    // Sort Ethernet/WLAN adapters first
    std::stable_sort(adapters.begin(), adapters.end(),
                     [](const QString &a, const QString &b) {
                         const auto isPhysical = [](const QString &name) {
                             const QString lower = name.toLower();
                             return lower.contains(QStringLiteral("ethernet"))
                                    || lower.contains(QStringLiteral("以太网"))
                                    || lower.contains(QStringLiteral("wlan"))
                                    || lower.contains(QStringLiteral("wi-fi"))
                                    || lower.contains(QStringLiteral("wifi"))
                                    || lower.contains(QStringLiteral("无线"));
                         };
                         return isPhysical(a) && !isPhysical(b);
                     });

    m_adapterComboBox->clear();
    m_adapterComboBox->addItems(adapters);

    const int index = m_adapterComboBox->findText(previous);
    if (index >= 0) {
        m_adapterComboBox->setCurrentIndex(index);
    }
}

void IpManagerWidget::refreshTable()
{
    const QList<IpRecord::Record> records = IpRecord::instance().getActiveRecords();
    m_tableWidget->setRowCount(records.size());

    for (int row = 0; row < records.size(); ++row) {
        const IpRecord::Record &record = records.at(row);
        m_tableWidget->setItem(row, 0, makeReadOnlyItem(record.macAddress));
        m_tableWidget->setItem(row, 1, makeReadOnlyItem(record.adapterName));
        m_tableWidget->setItem(row, 2, makeReadOnlyItem(record.assignedIp));
        m_tableWidget->setItem(row, 3, makeReadOnlyItem(record.hostname));
        m_tableWidget->setItem(row, 4, makeReadOnlyItem(record.assignedAt));
    }

    m_tableWidget->resizeColumnsToContents();
}

QString IpManagerWidget::currentAdapter() const
{
    return m_adapterComboBox->currentText().trimmed();
}

QString IpManagerWidget::findMacAddressForAdapter(const QString &adapter) const
{
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.humanReadableName() == adapter || iface.name() == adapter) {
            return iface.hardwareAddress();
        }
    }

    return QString();
}

void IpManagerWidget::setStatusMessage(const QString &text, const QString &color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-weight: 600; }").arg(color));
}
