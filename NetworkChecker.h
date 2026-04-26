#ifndef NETWORKCHECKER_H
#define NETWORKCHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class NetworkChecker : public QObject
{
    Q_OBJECT

public:
    explicit NetworkChecker(QObject *parent = nullptr);

public slots:
    void checkOnline();

signals:
    void statusChecked(bool isOnline);
    void networkError(QString msg);

private slots:
    void handleReplyFinished();
    void handleTimeout();

private:
    void finishCheck(bool isOnline, const QString &errorMessage = QString());
    void cleanupReply();

    QNetworkAccessManager *m_manager;
    QTimer *m_timeoutTimer;
    QNetworkReply *m_reply;
};

#endif // NETWORKCHECKER_H
