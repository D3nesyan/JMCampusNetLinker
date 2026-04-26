#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "IpManagerWidget.h"
#include "NetworkChecker.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStringList>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kMaxReloginAttempts = 3;
constexpr int kOnlineCheckIntervalMs = 60000;

QString toPowerShellSingleQuoted(const QString &value)
{
    QString escaped = value;
    escaped.replace('\'', QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(escaped);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_networkChecker(new NetworkChecker(this))
    , m_auth(new EportalAuth(this))
    , m_ipManagerWidget(new IpManagerWidget(this))
    , m_onlineCheckTimer(new QTimer(this))
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_trayMenu(new QMenu(this))
    , m_showAction(new QAction(QStringLiteral("显示窗口"), this))
    , m_quitAction(new QAction(QStringLiteral("退出程序"), this))
    , m_settings(QStringLiteral("JMCampusNetLinker"), QStringLiteral("JMCampusNetLinker"))
    , m_actionState(ActionState::None)
    , m_reloginAttempts(0)
    , m_isLoggedIn(false)
    , m_quitRequested(false)
{
    ui->setupUi(this);

    auto *ipTabLayout = new QVBoxLayout(ui->ipManagerTab);
    ipTabLayout->setContentsMargins(0, 0, 0, 0);
    ipTabLayout->addWidget(m_ipManagerWidget);

    ui->passwordEdit->setEchoMode(QLineEdit::Password);
    ui->serviceComboBox->addItems(QStringList{
            QStringLiteral("教育网接入"),
            QStringLiteral("电信宽带接入"),
            QStringLiteral("联通宽带接入"),
            QStringLiteral("移动宽带接入")
    });

    loadSettings();
    setStatusLabel(false, tr("Status: Idle"));
    updateWindowTitle(false);

    m_onlineCheckTimer->setInterval(kOnlineCheckIntervalMs);

    m_trayMenu->addAction(m_showAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_quitAction);

    m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_trayIcon->setToolTip(QStringLiteral("JiMei Campus NetLinker"));
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_showAction, &QAction::triggered, this, [this] {
        showNormal();
        raise();
        activateWindow();
    });

    connect(m_quitAction, &QAction::triggered, this, [this] {
        m_quitRequested = true;
        qApp->quit();
    });

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    toggleWindowVisibility();
                }
            });

    connect(ui->userIdEdit, &QLineEdit::textChanged, this,
            [this] { saveSettings(); });
    connect(ui->passwordEdit, &QLineEdit::editingFinished, this,
            [this] { saveSettings(); });
    connect(ui->serviceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { saveSettings(); });
    connect(ui->autoStartCheckBox, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState) { saveSettings(); });

    connect(ui->loginButton, &QPushButton::clicked, this, [this] {
        beginLogin(ActionState::Login);
    });

    connect(ui->logoutButton, &QPushButton::clicked, this, [this] {
        m_actionState = ActionState::Logout;
        stopOnlineMonitor();
        setStatusLabel(false, tr("Status: Logging out..."));
        updateButtonStates();
        m_auth->logout();
    });

    connect(m_auth, &EportalAuth::authSuccess, this, [this] {
        m_actionState = ActionState::None;
        m_isLoggedIn = true;
        m_reloginAttempts = 0;
        setStatusLabel(true, tr("Status: Online"));
        updateButtonStates();

        if (m_trayIcon->isVisible()) {
            m_trayIcon->showMessage(QStringLiteral("已连接"),
                                    QStringLiteral("校园网认证成功"),
                                    QSystemTrayIcon::Information,
                                    3000);
        }
    });

    connect(m_auth, &EportalAuth::authFailed, this, [this](const QString &reason) {
        const bool autoRelogin = m_actionState == ActionState::AutoRelogin;
        m_actionState = ActionState::None;

        if (autoRelogin) {
            handleOfflineDetected(reason);
            return;
        }

        m_isLoggedIn = false;
        setStatusLabel(false, tr("Status: Login failed (%1)").arg(reason));
        updateButtonStates();
    });

    connect(m_auth, &EportalAuth::logoutDone, this, [this](const QString &result) {
        m_actionState = ActionState::None;
        m_isLoggedIn = false;
        m_reloginAttempts = 0;
        stopOnlineMonitor();

        const QString text = result.isEmpty()
                ? tr("Status: Logged out")
                : tr("Status: Logged out (%1)").arg(result);
        setStatusLabel(false, text);
        updateButtonStates();
    });

    connect(m_onlineCheckTimer, &QTimer::timeout,
            m_networkChecker, &NetworkChecker::checkOnline);

    connect(m_networkChecker, &NetworkChecker::statusChecked, this,
            [this](bool isOnline) {
                if (isOnline) {
                    if (m_actionState == ActionState::None) {
                        setStatusLabel(true, tr("Status: Online"));
                    }
                    m_reloginAttempts = 0;
                    return;
                }

                if (m_isLoggedIn && m_actionState == ActionState::None) {
                    handleOfflineDetected(tr("Connection lost"));
                    return;
                }

                setStatusLabel(false, tr("Status: Unauthenticated"));
            });

    connect(m_networkChecker, &NetworkChecker::networkError, this,
            [this](const QString &msg) {
                if (m_actionState == ActionState::AutoRelogin) {
                    return;
                }

                if (m_isLoggedIn && m_actionState == ActionState::None) {
                    setStatusLabel(false, tr("Status: Unauthenticated (%1)").arg(msg));
                    return;
                }

                if (!m_isLoggedIn) {
                    setStatusLabel(false, tr("Status: Unauthenticated (%1)").arg(msg));
                }
            });

    updateButtonStates();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_quitRequested) {
        event->accept();
        return;
    }

    hide();
    event->ignore();
}

void MainWindow::setStatusLabel(bool isOnline, const QString &text)
{
    const QString color = isOnline ? "#1b8a3a" : "#c0392b";
    ui->statusLabel->setText(text);
    ui->statusLabel->setStyleSheet(
            QString("QLabel { color: %1; font-weight: 600; }").arg(color));
    updateWindowTitle(isOnline);
}

void MainWindow::loadSettings()
{
    ui->userIdEdit->setText(m_settings.value(QStringLiteral("userId")).toString());
    ui->passwordEdit->setText(
            decryptPwd(m_settings.value(QStringLiteral("encryptedPassword")).toString()));

    const int serviceIndex = m_settings.value(QStringLiteral("serviceIndex"), 0).toInt();
    ui->serviceComboBox->setCurrentIndex(qBound(0, serviceIndex, ui->serviceComboBox->count() - 1));

    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    ui->autoStartCheckBox->setChecked(reg.contains(QStringLiteral("CampusNetTool")));
}

void MainWindow::saveSettings()
{
    m_settings.setValue(QStringLiteral("userId"), ui->userIdEdit->text().trimmed());
    m_settings.setValue(QStringLiteral("encryptedPassword"),
                        encryptPwd(ui->passwordEdit->text()));
    m_settings.setValue(QStringLiteral("serviceIndex"), ui->serviceComboBox->currentIndex());

    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    if (ui->autoStartCheckBox->isChecked()) {
        reg.setValue(QStringLiteral("CampusNetTool"),
                     qApp->applicationFilePath() + QStringLiteral(" --minimized"));
    } else {
        reg.remove(QStringLiteral("CampusNetTool"));
    }
}

QString MainWindow::encryptPwd(QString plainText)
{
    if (plainText.isEmpty()) {
        return QString();
    }

    const QString command = QStringLiteral(
            "[Convert]::ToBase64String([Security.Cryptography.ProtectedData]::Protect("
            "[Text.Encoding]::UTF8.GetBytes(%1), $null, 'CurrentUser'))")
            .arg(toPowerShellSingleQuoted(plainText));

    QProcess process;
    process.start(QStringLiteral("powershell"), {QStringLiteral("-Command"), command});
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        return QString();
    }

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString MainWindow::decryptPwd(QString encryptedBase64)
{
    if (encryptedBase64.isEmpty()) {
        return QString();
    }

    const QString command = QStringLiteral(
            "[Text.Encoding]::UTF8.GetString([Security.Cryptography.ProtectedData]::Unprotect("
            "[Convert]::FromBase64String(%1), $null, 'CurrentUser'))")
            .arg(toPowerShellSingleQuoted(encryptedBase64));

    QProcess process;
    process.start(QStringLiteral("powershell"), {QStringLiteral("-Command"), command});
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        return QString();
    }

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

void MainWindow::updateButtonStates()
{
    const bool idle = m_actionState == ActionState::None;
    ui->loginButton->setEnabled(idle);
    ui->logoutButton->setEnabled(idle && m_isLoggedIn);
}

void MainWindow::beginLogin(ActionState state)
{
    const QString userId = ui->userIdEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    if (userId.isEmpty() || password.isEmpty()) {
        setStatusLabel(false, tr("Status: Enter student ID and password"));
        return;
    }

    saveSettings();

    m_actionState = state;
    if (state == ActionState::Login) {
        m_reloginAttempts = 0;
        setStatusLabel(false, tr("Status: Logging in..."));
    } else {
        setStatusLabel(false, tr("Status: Reconnecting (%1/%2)...")
                              .arg(m_reloginAttempts)
                              .arg(kMaxReloginAttempts));
    }

    updateButtonStates();
    m_auth->login(userId, password, currentServiceType());
}

void MainWindow::startOnlineMonitor()
{
    m_onlineCheckTimer->start();
}

void MainWindow::stopOnlineMonitor()
{
    m_onlineCheckTimer->stop();
}

void MainWindow::handleOfflineDetected(const QString &reason)
{
    stopOnlineMonitor();

    if (ui->userIdEdit->text().trimmed().isEmpty() || ui->passwordEdit->text().isEmpty()) {
        m_actionState = ActionState::None;
        m_isLoggedIn = false;
        setStatusLabel(false, tr("Status: Offline (%1)").arg(reason));
        QMessageBox::warning(this, tr("Reconnect Failed"),
                             tr("Stored credentials are incomplete. Please log in again."));
        updateButtonStates();
        return;
    }

    if (m_reloginAttempts >= kMaxReloginAttempts) {
        m_actionState = ActionState::None;
        m_isLoggedIn = false;
        setStatusLabel(false, tr("Status: Offline (%1)").arg(reason));
        QMessageBox::warning(this, tr("Reconnect Failed"),
                             tr("Auto re-login exceeded %1 attempts. Please log in manually.")
                                     .arg(kMaxReloginAttempts));
        if (m_trayIcon->isVisible()) {
            m_trayIcon->showMessage(QStringLiteral("连接失败"),
                                    QStringLiteral("请检查账号密码"),
                                    QSystemTrayIcon::Warning,
                                    5000);
        }
        updateButtonStates();
        return;
    }

    ++m_reloginAttempts;
    beginLogin(ActionState::AutoRelogin);
}

void MainWindow::updateWindowTitle(bool isOnline)
{
    setWindowTitle(isOnline
                   ? QStringLiteral("JiMei Campus NetLinker - 在线")
                   : QStringLiteral("JiMei Campus NetLinker - 未认证"));
}

void MainWindow::toggleWindowVisibility()
{
    if (isVisible()) {
        hide();
        return;
    }

    showNormal();
    raise();
    activateWindow();
}

EportalAuth::ServiceType MainWindow::currentServiceType() const
{
    switch (ui->serviceComboBox->currentIndex()) {
    case 0:
        return EportalAuth::ServiceType::Edu;
    case 1:
        return EportalAuth::ServiceType::Telecom;
    case 2:
        return EportalAuth::ServiceType::Unicom;
    case 3:
        return EportalAuth::ServiceType::Mobile;
    default:
        return EportalAuth::ServiceType::Edu;
    }
}
