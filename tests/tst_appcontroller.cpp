#include <QtTest>

#include "appcontroller.h"

class Tst_AppController : public QObject
{
    Q_OBJECT

private slots:
    void constructor()
    {
        AppController controller;
        QVERIFY(&controller != nullptr);
    }
};

QTEST_MAIN(Tst_AppController)
#include "tst_appcontroller.moc"
