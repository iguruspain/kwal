#include "aboutinfo.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

namespace {

// One person from the "Authors"/"Credits" arrays of aboutdata.json, in the
// shape Kirigami.AboutItem's person delegate reads; missing fields stay empty.
QVariantMap person(const QJsonObject &obj)
{
    return {
        { QStringLiteral("name"), obj.value("Name").toString() },
        { QStringLiteral("task"), obj.value("Text").toString() },
        { QStringLiteral("emailAddress"), obj.value("Email").toString() },
        { QStringLiteral("webAddress"), obj.value("Web").toString() },
    };
}

QVariantList people(const QJsonValue &value)
{
    QVariantList list;
    const auto array = value.toArray();
    for (const auto &entry : array) {
        if (entry.isObject()) {
            list.append(person(entry.toObject()));
        }
    }
    return list;
}

// The "License" field is a plain string (an SPDX identifier) or an object
// with a "Type" member; both map to the display name.
QString licenseType(const QJsonValue &value)
{
    if (value.isObject()) {
        return value.toObject().value("Type").toString();
    }
    return value.toString();
}

} // namespace

AboutInfo *AboutInfo::s_instance = nullptr;

void AboutInfo::initialize(QObject *parent)
{
    if (s_instance) {
        s_instance->deleteLater();
    }
    s_instance = new AboutInfo(parent);
}

AboutInfo *AboutInfo::instance()
{
    return s_instance;
}

AboutInfo::AboutInfo(QObject *parent)
    : QObject(parent)
{
    QFile file(QStringLiteral(":/aboutdata.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AboutInfo: cannot open :/aboutdata.json:" << file.errorString();
        return;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    const auto obj = doc.object();
    m_name = obj.value("Name").toString();
    m_genericName = obj.value("GenericName").toString();
    m_description = obj.value("Description").toString();
    m_version = obj.value("Version").toString();
    m_homepage = obj.value("Homepage").toString();
    m_id = obj.value("Id").toString();
    m_licenseName = licenseType(obj.value("License"));
    m_copyright = obj.value("Copyright").toString();
    m_authors = people(obj.value("Authors"));
    m_credits = people(obj.value("Credits"));
}

QVariantMap AboutInfo::aboutData() const
{
    QVariantList licenses;
    licenses.append(QVariantMap{
        { QStringLiteral("name"), m_licenseName },
        { QStringLiteral("spdx"), m_licenseName },
        { QStringLiteral("text"), licenseText() },
    });

    return {
        { QStringLiteral("displayName"), m_name },
        { QStringLiteral("productName"), QStringLiteral("kwal") },
        { QStringLiteral("componentName"), QStringLiteral("kwal") },
        { QStringLiteral("shortDescription"), m_description },
        { QStringLiteral("homepage"), m_homepage },
        { QStringLiteral("bugAddress"), QStringLiteral("submit@bugs.kde.org") },
        { QStringLiteral("version"), m_version },
        { QStringLiteral("otherText"), m_genericName },
        { QStringLiteral("authors"), m_authors },
        { QStringLiteral("credits"), m_credits },
        { QStringLiteral("translators"), QVariantList() },
        { QStringLiteral("licenses"), licenses },
        { QStringLiteral("copyrightStatement"), m_copyright },
        { QStringLiteral("desktopFileName"), m_id },
        { QStringLiteral("programLogo"), QStringLiteral("qrc:/kwal.svg") },
    };
}

QString AboutInfo::licenseText() const
{
    if (m_licenseText.isEmpty()) {
        QFile file(QStringLiteral(":/LICENSE.txt"));
        if (file.open(QIODevice::ReadOnly)) {
            m_licenseText = QString::fromUtf8(file.readAll());
        }
    }
    return m_licenseText;
}
