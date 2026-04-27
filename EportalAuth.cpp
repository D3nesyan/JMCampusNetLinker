#include "EportalAuth.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {

// Standard captive-portal detection URL used by Chrome / Android.
// Campus portals (including JMU) intercept this and redirect to the login page.
constexpr auto kProbeUrl = "http://connectivitycheck.gstatic.com/generate_204";
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
    m_userId = userId;
    m_password = password;
    m_service = service;
    m_probeFallback = 0;
    m_probeRetries = 0;
    startProbe();
}

void EportalAuth::startProbe()
{
    cleanupReply();

    // Try multiple probe URLs in sequence (controlled by m_probeFallback).
    // 0 = standard captive-portal detection URL
    // 1 = direct portal access (may redirect to full login page)
    // 2 = Windows NCSI detection URL
    // 3 = portal logout-probe endpoint (may redirect with userIndex)
    QUrl probeUrl;
    if (m_probeFallback == 0) {
        probeUrl = QUrl(QString::fromLatin1(kProbeUrl));
    } else if (m_probeFallback == 1) {
        probeUrl = QUrl(QStringLiteral("http://10.8.2.2/eportal/index.jsp"));
    } else if (m_probeFallback == 2) {
        probeUrl = QUrl(QStringLiteral("http://www.msftconnecttest.com/connecttest.txt"));
    } else {
        probeUrl = QUrl(QStringLiteral("http://10.8.2.2/eportal/redirectortosuccess.jsp"));
    }

    qDebug() << "[EportalAuth] Probing (fallback level" << m_probeFallback << "):" << probeUrl.toString();

    QNetworkRequest request{probeUrl};
    request.setRawHeader("User-Agent", QByteArray(kUserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    m_reply = m_manager->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &EportalAuth::handleProbeFinished);
}

void EportalAuth::cancel()
{
    cleanupReply();
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
    const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString redirectAttr =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();

    qDebug() << "[EportalAuth] Probe finished"
             << "url:" << reply->url().toString()
             << "httpStatus:" << httpStatus
             << "error:" << reply->error()
             << "errorString:" << reply->errorString()
             << "redirectAttribute:" << redirectAttr
             << "bodySize:" << body.size()
             << "headers:" << reply->rawHeaderPairs();
    if (!body.isEmpty()) {
        // The portal often uses GBK; try local 8-bit (GBK on Chinese Windows)
        // first, then UTF-8.
        const QString bodyText = QString::fromLocal8Bit(body.left(600));
        qDebug() << "[EportalAuth] Probe body (first 600 chars):" << bodyText;
    }

    const QString loginPageUrl = extractLoginPageUrl(reply, body);
    qDebug() << "[EportalAuth] Extracted loginPageUrl:" << loginPageUrl;
    if (loginPageUrl.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
            if (m_probeFallback < 3) {
                qDebug() << "[EportalAuth] Probe level" << m_probeFallback
                         << "failed, trying level" << (m_probeFallback + 1);
                ++m_probeFallback;
                startProbe();
                return;
            }
            fail(tr("All probe URLs failed. "
                    "Make sure you are connected to campus WiFi. "
                    "Try opening any website in a browser first, then retry."));
            return;
        }
        // HTTP succeeded but no login URL found — the body is probably
        // an error page (e.g. missing query parameters).  Try next fallback.
        if (m_probeFallback < 3) {
            qDebug() << "[EportalAuth] Probe level" << m_probeFallback
                     << "got 200 but no login URL, trying level" << (m_probeFallback + 1);
            ++m_probeFallback;
            startProbe();
            return;
        }
        fail(tr("Failed to extract login page URL"));
        return;
    }

    QUrl loginPage(loginPageUrl);
    if (!loginPage.isValid() || !loginPage.path().contains(QStringLiteral("index.jsp"))) {
        // The portal returned a redirect that is not the real login page
        // (e.g. 123.123.123.123).  Retry from a known-good probe level.
        constexpr int kMaxProbeRetries = 8;
        qDebug() << "[EportalAuth] Invalid login page URL, retry" << m_probeRetries;
        if (++m_probeRetries < kMaxProbeRetries) {
            m_probeFallback = 2;  // go back to the level that normally works
            startProbe();
            return;
        }
        fail(tr("Portal returned an invalid redirect. "
                "The campus network may be unstable — try again in a moment."));
        return;
    }

    const QString loginPagePath = loginPage.path();
    QString loginUrl;
    const QString loginPath = QString(loginPagePath).replace(
            QStringLiteral("index.jsp"),
            QStringLiteral("InterFace.do"));
    QUrl postUrl(loginPage);
    postUrl.setPath(loginPath);
    postUrl.setQuery(QStringLiteral("method=login"));
    loginUrl = postUrl.toString(QUrl::FullyEncoded);

    const QString encodedQuery = encodeQueryString(loginPage.query(QUrl::FullyEncoded));
    const QByteArray bodyData = buildFormBody(
            m_userId,
            m_password,
            serviceValue(m_service),
            encodedQuery).toUtf8();

    cleanupReply();

    qDebug() << "[EportalAuth] POST login to:" << loginUrl;
    qDebug() << "[EportalAuth] loginPageUrl:" << loginPageUrl;
    qDebug() << "[EportalAuth] encodedQuery:" << encodedQuery;

    QNetworkRequest request{QUrl(loginUrl)};
    request.setRawHeader("Content-Type",
                         "application/x-www-form-urlencoded; charset=UTF-8");
    request.setRawHeader("Accept",
                         "text/html,application/xhtml+xml,application/xml;"
                         "q=0.9,image/webp,image/apng,*/*;q=0.8");
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
    const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug() << "[EportalAuth] Login POST finished"
             << "httpStatus:" << httpStatus
             << "error:" << reply->error()
             << "errorString:" << reply->errorString()
             << "bodySize:" << body.size();
    if (!body.isEmpty()) {
        qDebug() << "[EportalAuth] Login response body:"
                 << QString::fromLocal8Bit(body.left(500));
    }

    if (reply->error() != QNetworkReply::NoError) {
        fail(reply->errorString());
        return;
    }

    cleanupReply();

    // Parse the response — the eportal returns JSON with "result" / "message"
    // fields, matching what the shell script extracts with awk.
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString result = obj.value(QStringLiteral("result")).toString();
        const QString message = obj.value(QStringLiteral("message")).toString();
        qDebug() << "[EportalAuth] Login response JSON — result:"
                 << result << "message:" << message;

        // The shell script treats an empty/non-existent "result" or "success"
        // value as success.  Many eportal deployments return result="success"
        // or result="" on success, and result="fail" / non-empty message on
        // failure.
        if (!result.isEmpty()
            && result != QStringLiteral("success")
            && result != QStringLiteral("ok")) {
            const QString reason = message.isEmpty() ? result : message;
            fail(reason);
            return;
        }
        if (!message.isEmpty()
            && message != QStringLiteral("success")
            && message != QStringLiteral("ok")) {
            fail(message);
            return;
        }
    }

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
    // 1) HTTP redirect (302) — portal redirected us to the login page.
    const QVariant redirectTarget =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirectTarget.isValid()) {
        const QUrl redirectUrl = reply->url().resolved(redirectTarget.toUrl());
        if (redirectUrl.isValid()) {
            return redirectUrl.toString(QUrl::FullyEncoded);
        }
    }

    // The portal may use GBK encoding; check the Content-Type header.
    const QString contentType =
            reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const bool isGbk = contentType.contains(QStringLiteral("GBK"), Qt::CaseInsensitive)
                       || contentType.contains(QStringLiteral("GB2312"), Qt::CaseInsensitive);
    const QString text = isGbk ? QString::fromLocal8Bit(body) : QString::fromUtf8(body);

    // 2) Match the shell script's `awk -F \' '{print $2}'` — the portal
    //    typically embeds the login URL inside single quotes (meta-refresh /
    //    JavaScript redirect).  Walk every single-quoted segment.
    const QStringList fields = text.split(QLatin1Char('\''));
    for (const QString &field : fields) {
        if (field.contains(QStringLiteral("index.jsp"))) {
            return field;
        }
    }

    // 3) Regex fallback — a URL containing "index.jsp" anywhere in the raw HTML.
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
