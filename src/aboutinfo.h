#pragma once

#include <QVariantMap>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// About data for the settings window's About page (Kirigami.AboutPage).
// Loaded once at startup from the bundled aboutdata.json (the typical KDE
// layout): edit that file to change the name, version, description,
// homepage, license, copyright and authors.
class AboutInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    QML_UNCREATABLE("Provided by C++")
public:
    // Creates (or replaces) the singleton.
    static void initialize(QObject *parent);
    static AboutInfo *instance();
    // The KAboutData-shaped object consumed by Kirigami.AboutPage::aboutData
    // (QML auto-converts the map and its nested lists/maps to JS values).
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
