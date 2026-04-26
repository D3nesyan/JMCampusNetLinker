#ifndef EPORTALAUTH_H
#define EPORTALAUTH_H

#include <QObject>
#include <QString>

class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;

class EportalAuth : public QObject
{
    Q_OBJECT

public:
    enum class ServiceType {
        Edu,
        Telecom,
        Unicom,
        Mobile
    };
    Q_ENUM(ServiceType)

    explicit EportalAuth(QObject *parent = nullptr);

    void login(QString userId, QString password, ServiceType service);
    void logout();

signals:
    void authSuccess();
    void authFailed(QString reason);
    void logoutDone(QString result);

private slots:
    void handleProbeFinished();
    void handleLoginFinished();
    void handleLogoutProbeFinished();
    void handleLogoutFinished();

private:
    static QString serviceValue(ServiceType service);
    static QString encodeQueryString(const QString &queryString);
    static QString extractLoginPageUrl(const QNetworkReply *reply, const QByteArray &body);
    static QString extractUserIndex(const QNetworkReply *reply);
    static QString buildFormBody(const QString &userId,
                                 const QString &password,
                                 const QString &service,
                                 const QString &queryString);
    void fail(const QString &reason);
    void cleanupReply();

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    QString m_userId;
    QString m_password;
    ServiceType m_service;
};

#endif // EPORTALAUTH_H
