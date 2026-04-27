#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "EportalAuth.h"

#include <QMainWindow>
#include <QSettings>
#include <QString>

class IpManagerWidget;
class NetworkChecker;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    enum class ActionState {
        None,
        Login,
        AutoRelogin,
        Logout
    };

    void setStatusLabel(bool isOnline, const QString &text);
    void loadSettings();
    void saveSettings();
    QString encryptPwd(QString plainText);
    QString decryptPwd(QString encryptedBase64);
    void updateButtonStates();
    void beginLogin(ActionState state);
    void startOnlineMonitor();
    void stopOnlineMonitor();
    void handleOfflineDetected(const QString &reason);

    EportalAuth::ServiceType currentServiceType() const;

    Ui::MainWindow *ui;
    NetworkChecker *m_networkChecker;
    EportalAuth *m_auth;
    IpManagerWidget *m_ipManagerWidget;
    QTimer *m_onlineCheckTimer;
    QSettings m_settings;
    ActionState m_actionState;
    int m_reloginAttempts;
    bool m_isLoggedIn;
    bool m_loading = true;
};

#endif // MAINWINDOW_H
