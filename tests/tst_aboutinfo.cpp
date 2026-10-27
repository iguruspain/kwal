#include "aboutinfo.h"

#include <QtTest/QtTest>

// The About page (Kirigami.AboutPage) renders whatever aboutdata.json holds;
// these checks pin the aboutData() object shape and the bundled resources so
// a broken edit of the JSON is caught here instead of as an empty page.
class tst_AboutInfo : public QObject
{
    Q_OBJECT

private slots:
    void testFields();
};

void tst_AboutInfo::testFields()
{
    AboutInfo::initialize(nullptr);
    AboutInfo *about = AboutInfo::instance();
    QVERIFY(about);

    const auto data = about->aboutData();
    QCOMPARE(data.value("displayName").toString(), QStringLiteral("Kwal"));
    QVERIFY(!data.value("shortDescription").toString().isEmpty());
    QVERIFY(!data.value("version").toString().isEmpty());
    QVERIFY(!data.value("homepage").toString().isEmpty());
    QCOMPARE(data.value("desktopFileName").toString(), QStringLiteral("org.kde.kwal"));
    QVERIFY(!data.value("copyrightStatement").toString().isEmpty());

    const auto licenses = data.value("licenses").toList();
    QCOMPARE(licenses.size(), 1);
    const auto license = licenses.first().toMap();
    QCOMPARE(license.value("spdx").toString(), QStringLiteral("GPL-3.0-or-later"));
    const auto text = license.value("text").toString();
    QVERIFY(!text.isEmpty());
    QVERIFY(text.contains("GNU General Public License"));
    QVERIFY(text.contains("SPDX-License-Identifier: GPL-3.0-or-later"));

    const auto authors = data.value("authors").toList();
    QVERIFY(authors.size() >= 1);
    const auto author = authors.first().toMap();
    QVERIFY(!author.value("name").toString().isEmpty());
    QVERIFY(!author.value("task").toString().isEmpty());
}

QTEST_GUILESS_MAIN(tst_AboutInfo)
#include "tst_aboutinfo.moc"
