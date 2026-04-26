#include "EportalAuth.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr auto kProbeUrl = "http://g.cn/generate_204";
constexpr auto kLogoutProbeUrl = "http://10.8.2.2/eportal/redirectortosuccess.jsp";
constexpr auto kLogoutUrl = "http://10.8.2.2/eportal/InterFace.do?method=logout";
constexpr auto kUserAgent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/72.0.3626.121 Safari/537.36";
constexpr auto kCookieHeader =
        "EPORTAL_COOKIE_USERNAME=; "
        "EPORTAL_COOKIE_PASSWORD=; "
        "EPORTAL_COOKIE_SERVER=; "
        "EPORTAL_COOKIE_SERVER_NAME=; "
        "EPORTAL_AUTO_LAND=; "
        "EPORTAL_USER_GROUP=%E5%AD%A6%E7%94%9F%E5%8C%85%E6%9C%88; "
        "EPORTAL_COOKIE_OPERATORPWD=;";

QString percentEncodeFormValue(const QString &value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

} // namespace

EportalAuth::EportalAuth(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_service(ServiceType::Edu)
{
}

void EportalAuth::login(QString userId, QString password, ServiceType service)
{
    cleanupReply();

    m_userId = userId;
    m_password = password;
    m_service = service;

    QNetworkRequest request{QUrl(QString::fromLatin1(kProbeUrl))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    m_reply = m_manager->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &EportalAuth::handleProbeFinished);
}

void EportalAuth::logout()
{
    cleanupReply();

    QNetworkRequest request{QUrl(QString::fromLatin1(kLogoutProbeUrl))};
    request.setRawHeader("User-Agent", QByteArray(kUserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    m_reply = m_manager->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &EportalAuth::handleLogoutProbeFinished);
}

void EportalAuth::handleProbeFinished()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply *reply = m_reply;
    const QByteArray body = reply->readAll();

    const QString loginPageUrl = extractLoginPageUrl(reply, body);
    if (loginPageUrl.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
            fail(reply->errorString());
            return;
        }
        fail(tr("Failed to extract login page URL"));
        return;
    }

    QUrl loginPage(loginPageUrl);
    if (!loginPage.isValid()) {
        fail(tr("Invalid login page URL"));
        return;
    }

    const QString loginPagePath = loginPage.path();
    QString loginUrl;
    if (loginPagePath.contains(QStringLiteral("index.jsp"))) {
        const QString loginPath = QString(loginPagePath).replace(
                QStringLiteral("index.jsp"),
                QStringLiteral("InterFace.do"));
        QUrl postUrl(loginPage);
        postUrl.setPath(loginPath);
        postUrl.setQuery(QStringLiteral("method=login"));
        loginUrl = postUrl.toString(QUrl::FullyEncoded);
    } else {
        fail(tr("Login page URL does not contain index.jsp"));
        return;
    }

    const QString encodedQuery = encodeQueryString(loginPage.query(QUrl::FullyEncoded));
    const QByteArray bodyData = buildFormBody(
            m_userId,
            m_password,
            serviceValue(m_service),
            encodedQuery).toUtf8();

    cleanupReply();

    QNetworkRequest request{QUrl(loginUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("Cookie", QByteArray(kCookieHeader));
    request.setRawHeader("User-Agent", QByteArray(kUserAgent));
    request.setRawHeader("Referer", loginPageUrl.toUtf8());

    m_reply = m_manager->post(request, bodyData);
    connect(m_reply, &QNetworkReply::finished, this, &EportalAuth::handleLoginFinished);
}

void EportalAuth::handleLoginFinished()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply *reply = m_reply;
    const QByteArray body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        fail(reply->errorString());
        return;
    }

    cleanupReply();

    // The eportal login POST itself is sufficient — if the HTTP request
    // succeeded the campus network considers us authenticated.
    emit authSuccess();
}

void EportalAuth::handleLogoutProbeFinished()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply *reply = m_reply;
    const QString userIndex = extractUserIndex(reply);
    if (userIndex.isEmpty()) {
        const QString reason = reply->error() == QNetworkReply::NoError
                ? tr("Failed to extract userIndex")
                : reply->errorString();
        cleanupReply();
        emit logoutDone(reason);
        return;
    }

    cleanupReply();

    QNetworkRequest request{QUrl(QString::fromLatin1(kLogoutUrl))};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", QByteArray(kUserAgent));

    const QByteArray bodyData =
            QStringLiteral("userIndex=%1").arg(percentEncodeFormValue(userIndex)).toUtf8();

    m_reply = m_manager->post(request, bodyData);
    connect(m_reply, &QNetworkReply::finished, this, &EportalAuth::handleLogoutFinished);
}

void EportalAuth::handleLogoutFinished()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply *reply = m_reply;
    const QByteArray body = reply->readAll();
    QString result;

    if (reply->error() != QNetworkReply::NoError) {
        result = reply->errorString();
    } else {
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            result = obj.value(QStringLiteral("result")).toString();
            if (result.isEmpty()) {
                result = obj.value(QStringLiteral("message")).toString();
            }
        }

        if (result.isEmpty()) {
            result = QString::fromUtf8(body).trimmed();
        }
    }

    cleanupReply();
    emit logoutDone(result);
}

QString EportalAuth::serviceValue(ServiceType service)
{
    switch (service) {
    case ServiceType::Edu:
        return QStringLiteral("%25E6%2595%2599%25E8%2582%25B2%25E7%25BD%2591%25E6%258E%25A5%25E5%2585%25A5");
    case ServiceType::Telecom:
        return QStringLiteral("%25E7%2594%25B5%25E4%25BF%25A1%25E5%25AE%25BD%25E5%25B8%25A6%25E6%258E%25A5%25E5%2585%25A5");
    case ServiceType::Unicom:
        return QStringLiteral("%25E8%2581%2594%25E9%2580%259A%25E5%25AE%25BD%25E5%25B8%25A6%25E6%258E%25A5%25E5%2585%25A5");
    case ServiceType::Mobile:
        return QStringLiteral("%25E7%25A7%25BB%25E5%258A%25A8%25E5%25AE%25BD%25E5%25B8%25A6%25E6%258E%25A5%25E5%2585%25A5");
    }

    return QString();
}

QString EportalAuth::encodeQueryString(const QString &queryString)
{
    QString encoded = queryString;
    encoded.replace(QStringLiteral("&"), QStringLiteral("%2526"));
    encoded.replace(QStringLiteral("="), QStringLiteral("%253D"));
    return encoded;
}

QString EportalAuth::extractLoginPageUrl(const QNetworkReply *reply, const QByteArray &body)
{
    const QVariant redirectTarget =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirectTarget.isValid()) {
        const QUrl redirectUrl = reply->url().resolved(redirectTarget.toUrl());
        if (redirectUrl.isValid()) {
            return redirectUrl.toString(QUrl::FullyEncoded);
        }
    }

    const QString text = QString::fromUtf8(body);
    const QRegularExpression re(
            QStringLiteral(R"((https?://[^\s"'<>]+index\.jsp[^\s"'<>]*))"),
            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return QString();
}

QString EportalAuth::extractUserIndex(const QNetworkReply *reply)
{
    const QVariant redirectTarget =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirectTarget.isValid()) {
        const QUrl redirectUrl = reply->url().resolved(redirectTarget.toUrl());
        const QString value =
                QUrlQuery(redirectUrl).queryItemValue(QStringLiteral("userIndex"),
                                                     QUrl::FullyDecoded);
        if (!value.isEmpty()) {
            return value;
        }
    }

    const QUrl finalUrl = reply->url();
    const QString value =
            QUrlQuery(finalUrl).queryItemValue(QStringLiteral("userIndex"),
                                               QUrl::FullyDecoded);
    if (!value.isEmpty()) {
        return value;
    }

    return QString();
}

QString EportalAuth::buildFormBody(const QString &userId,
                                   const QString &password,
                                   const QString &service,
                                   const QString &queryString)
{
    return QStringLiteral("userId=%1&password=%2&service=%3&queryString=%4"
                          "&operatorPwd=&operatorUserId=&validcode=&passwordEncrypt=false")
            .arg(percentEncodeFormValue(userId),
                 percentEncodeFormValue(password),
                 service,
                 queryString);
}

void EportalAuth::fail(const QString &reason)
{
    cleanupReply();
    emit authFailed(reason);
}

void EportalAuth::cleanupReply()
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
