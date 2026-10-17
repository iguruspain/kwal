#pragma once

#include <QObject>

// Application state and services, exposed to QML as the "app" context property.
class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);
};
