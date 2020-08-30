#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <QQuickItem>
#include <QObject>
#include <fstream>
#include "controller.h"
#include "eventfilter.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

bool exists(const std::string& name) {
    std::fstream file(name.c_str());
    return file.good();
}

const char *qt_version = qVersion();

int main(int argc, char *argv[]){
    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "rotate=180");
    qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
//    qputenv("QT_DEBUG_BACKINGSTORE", "1");
#endif
    system("killall button-capture");
    if(exists("/opt/bin/button-capture")){
        qDebug() << "Starting button-capture";
        system("/opt/bin/button-capture &");
    }else{
        qDebug() << "button-capture not found or is running";
    }
    QGuiApplication app(argc, argv);
    EventFilter filter;
    app.installEventFilter(&filter);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller controller;
    controller.killXochitl();
    controller.filter = &filter;
    qmlRegisterType<AppItem>();
    qmlRegisterType<Controller>();
    controller.loadSettings();
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("apps", QVariant::fromValue(controller.getApps()));
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    controller.root = root;
    filter.root = (QQuickItem*)root;
    QQuickItem* appsView = root->findChild<QQuickItem*>("appsView");
    if(!appsView){
        qDebug() << "Can't find appsView";
        return -1;
    }
    QObject* stateController = root->findChild<QObject*>("stateController");
    if(!stateController){
        qDebug() << "Can't find stateController";
        return -1;
    }
    QObject* batteryLevel = root->findChild<QObject*>("batteryLevel");
    if(!stateController){
        qDebug() << "Can't find batteryLevel";
        return -1;
    }
    filter.timer = new QTimer(root);
    filter.timer->setInterval(5 * 60 * 1000); // 5 minutes
    QObject::connect(filter.timer, &QTimer::timeout, [stateController](){
        qDebug() << "Suspending due to inactivity...";
        stateController->setProperty("state", QString("suspending"));
    });
    QTimer* timer = new QTimer(root);
    timer->setInterval(10 * 60 * 1000); // 10 minutes
    QObject::connect(timer, &QTimer::timeout, [batteryLevel, &controller](){
        batteryLevel->setProperty("batterylevel", controller.getBatteryLevel());
    });
    timer->start();
    return app.exec();
}
