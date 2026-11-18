#pragma once

#include <QVariantMap>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// About page data, loaded once from the bundled aboutdata.json; edit that file to change it.
class AboutInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    QML_UNCREATABLE("Provided by C++")
public:
    static void initialize(QObject *parent);
    static AboutInfo *instance();
    // KAboutData-shaped map for Kirigami.AboutPage::aboutData.
    Q_INVOKABLE QVariantMap aboutData() const;

private:
    explicit AboutInfo(QObject *parent = nullptr);
    QString licenseText() const;

    QString m_name;
    QString m_genericName;
    QString m_description;
    QString m_version;
    QString m_homepage;
    QString m_id;
    QString m_licenseName;
    QString m_copyright;
    QVariantList m_authors;
    QVariantList m_credits;
    mutable QString m_licenseText;

    static AboutInfo *s_instance;
};
