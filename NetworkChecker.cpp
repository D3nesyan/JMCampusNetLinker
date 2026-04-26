#include "NetworkChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

NetworkChecker::NetworkChecker(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_timeoutTimer(new QTimer(this))
    , m_reply(nullptr)
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout,
            this, &NetworkChecker::handleTimeout);
}

void NetworkChecker::checkOnline()
{
    cleanupReply();

    // g.cn is blocked in mainland China; use a China-accessible URL instead.
    QNetworkRequest request(QUrl(QStringLiteral("https://www.baidu.com")));
    m_reply = m_manager->get(request);

    connect(m_reply, &QNetworkReply::finished,
            this, &NetworkChecker::handleReplyFinished);

    m_timeoutTimer->start(10000);
}

void NetworkChecker::handleReplyFinished()
{
    if (!m_reply) {
        return;
    }

    const QNetworkReply::NetworkError error = m_reply->error();
    const int statusCode =
            m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (error == QNetworkReply::NoError && statusCode == 200) {
        finishCheck(true);
        return;
    }

    QString message;
    if (error != QNetworkReply::NoError) {
        message = m_reply->errorString();
    } else {
        message = tr("HTTP %1").arg(statusCode);
    }

    finishCheck(false, message);
}

void NetworkChecker::handleTimeout()
{
    if (!m_reply) {
        return;
    }

    disconnect(m_reply, &QNetworkReply::finished,
               this, &NetworkChecker::handleReplyFinished);
    m_reply->abort();
    finishCheck(false, tr("Request timed out"));
}

void NetworkChecker::finishCheck(bool isOnline, const QString &errorMessage)
{
    m_timeoutTimer->stop();
    cleanupReply();

    emit statusChecked(isOnline);
    if (!isOnline && !errorMessage.isEmpty()) {
        emit networkError(errorMessage);
    }
}

void NetworkChecker::cleanupReply()
{
    if (!m_reply) {
        return;
    }

    disconnect(m_reply, nullptr, this, nullptr);
    if (m_reply->isRunning()) {
        m_reply->abort();
    }
    m_reply->deleteLater();
    m_reply = nullptr;
}
