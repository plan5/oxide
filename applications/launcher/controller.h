#pragma once

#include <QObject>
#include <QJsonObject>

#include "appitem.h"
#include "eventfilter.h"
#include "sysobject.h"
#include "inputmanager.h"
#include "wifimanager.h"
#include "dbusservice_interface.h"
#include "powerapi_interface.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };

class Controller : public QObject
{
    Q_OBJECT
public:
    static std::string exec(const char* cmd);
    EventFilter* filter;
    WifiManager* wifiManager;
    QObject* root = nullptr;
    explicit Controller(QObject* parent = 0) : QObject(parent), wifi("/sys/class/net/wlan0"), inputManager(){
        uiTimer = new QTimer(this);
        uiTimer->setSingleShot(false);
        uiTimer->setInterval(3 * 1000); // 3 seconds
        connect(uiTimer, &QTimer::timeout, this, QOverload<>::of(&Controller::updateUIElements));
        uiTimer->start();
        auto bus = QDBusConnection::systemBus();
        qDebug() << "Waiting for tarnish to start up";
        while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            usleep(1000);
        }
        General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        auto reply = api.requestAPI("power");
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << reply.error();
            qFatal("Could not request power API");
        }
        auto path = ((QDBusObjectPath)reply).path();
        if(path == "/"){
            qDebug() << "API not available";
            qFatal("Power API was not available");
        }
        powerApi = new Power(OXIDE_SERVICE, path, bus);
        connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
        connect(powerApi, &Power::batteryLevelChanged, this, &Controller::batteryLevelChanged);
        connect(powerApi, &Power::batteryStateChanged, this, &Controller::batteryStateChanged);
        connect(powerApi, &Power::batteryTemperatureChanged, this, &Controller::batteryTemperatureChanged);
        connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
        connect(powerApi, &Power::chargerStateChanged, this, &Controller::chargerStateChanged);
        connect(powerApi, &Power::chargerWarning, this, &Controller::chargerWarning);
        connect(powerApi, &Power::stateChanged, this, &Controller::stateChanged);
    }
    Q_INVOKABLE bool turnOnWifi(){
        system("ifconfig wlan0 up");
        if(wifiManager == nullptr){
            if(!WifiManager::ensureService()){
                return false;
            }
            wifiManager = WifiManager::singleton();
            wifiState = "up";
            wifiConnected = false;
            QObject* ui = root->findChild<QObject*>("wifiState");
            if(ui){
                ui->setProperty("state", wifiState);
                ui->setProperty("connected", wifiConnected);
            }
        }else{
            wifiManager->loadNetworks();
        }
        return true;
    };
    Q_INVOKABLE bool turnOffWifi(){
        if(wifiManager != nullptr){
            delete wifiManager;
            wifiManager = nullptr;
            system("killall wpa_supplicant");
        }
        system("ifconfig wlan0 down");
        wifiState = "down";
        wifiConnected = false;
        QObject* ui = root->findChild<QObject*>("wifiState");
        if(ui){
            ui->setProperty("state", wifiState);
            ui->setProperty("connected", wifiConnected);
        }
        return true;
    };
    Q_INVOKABLE bool wifiOn(){
        return wifiManager != nullptr;
    };
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void killXochitl();
    void updateBatteryLevel();
    void updateWifiState();
    Q_INVOKABLE void resetInactiveTimer();
    Q_PROPERTY(bool automaticSleep MEMBER m_automaticSleep WRITE setAutomaticSleep NOTIFY automaticSleepChanged);
    bool automaticSleep() const {
        return m_automaticSleep;
    };
    void setAutomaticSleep(bool);
    Q_PROPERTY(int columns MEMBER m_columns WRITE setColumns NOTIFY columnsChanged);
    int columns() const {
        return m_columns;
    };
    void setColumns(int);
    Q_PROPERTY(int fontSize MEMBER m_fontSize WRITE setFontSize NOTIFY fontSizeChanged);
    int fontSize() const {
        return m_fontSize;
    };
    void setFontSize(int);
    Q_PROPERTY(bool showWifiDb MEMBER m_showWifiDb WRITE setShowWifiDb NOTIFY showWifiDbChanged);
    bool showWifiDb() const {
        return m_showWifiDb;
    };
    void setShowWifiDb(bool);
    Q_PROPERTY(bool showBatteryPercent MEMBER m_showBatteryPercent WRITE setShowBatteryPercent NOTIFY showBatteryPercentChanged);
    bool showBatteryPercent() const {
        return m_showBatteryPercent;
    };
    void setShowBatteryPercent(bool);
    Q_PROPERTY(bool showBatteryTemperature MEMBER m_showBatteryTemperature WRITE setShowBatteryTemperature NOTIFY showBatteryTemperatureChanged);
    bool showBatteryTemperature() const {
        return m_showBatteryTemperature;
    };
    void setShowBatteryTemperature(bool);
    Q_PROPERTY(int sleepAfter MEMBER m_sleepAfter WRITE setSleepAfter NOTIFY sleepAfterChanged);
    int sleepAfter() const {
        return m_sleepAfter;
    };
    void setSleepAfter(int);
    bool getBatteryCharging(){
        return m_batteryCharging;
    }
signals:
    void automaticSleepChanged(bool);
    void columnsChanged(int);
    void fontSizeChanged(int);
    void showWifiDbChanged(bool);
    void showBatteryPercentChanged(bool);
    void showBatteryTemperatureChanged(bool);
    void sleepAfterChanged(int);
private slots:
    void updateUIElements();
    void batteryAlert(){
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("alert", true);
        }
    }
    void batteryLevelChanged(int level){
        qDebug() << "Battery level: " << level;
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("level", level);
        }
    }
    void batteryStateChanged(int state){
        switch(state){
            case BatteryCharging:
                qDebug() << "Battery state: Charging";
            break;
            case BatteryNotPresent:
                qDebug() << "Battery state: Not Present";
            break;
            case BatteryDischarging:
                qDebug() << "Battery state: Discharging";
            break;
            case BatteryUnknown:
            default:
                qDebug() << "Battery state: Unknown";
        }
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            if(state != BatteryNotPresent){
                ui->setProperty("present", true);
            }
            switch(state){
                case BatteryCharging:
                    ui->setProperty("charging", true);
                break;
                case BatteryNotPresent:
                    ui->setProperty("present", false);
                break;
                case BatteryDischarging:
                    ui->setProperty("charging", false);
                break;
                case BatteryUnknown:
                default:
                    ui->setProperty("warning", true);
                    ui->setProperty("charging", false);
            }
        }
    }
    void batteryTemperatureChanged(int temperature){
        qDebug() << "Battery temperature: " << temperature;
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("temperature", temperature);
        }
    }
    void batteryWarning(){
        qDebug() << "Battery Warning!";
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("warning", true);
        }
    }
    void chargerStateChanged(int state){
        Q_UNUSED(state);
        // TODO handle charger
    }
    void chargerWarning(){
        // TODO handle charger
    }
    void stateChanged(int state){
        Q_UNUSED(state);
        // TODO handle requested battery state
    }
private:
    void updateHiddenUIElements();
    void checkUITimer();
    bool m_automaticSleep = true;
    int m_columns = 6;
    int m_fontSize = 23;
    bool m_showWifiDb = false;
    bool m_showBatteryPercent = false;
    bool m_showBatteryTemperature = false;
    int m_sleepAfter = 5;
    bool m_batteryCharging = false;

    QString wifiState = "Unknown";
    int wifiLink = 0;
    int wifiLevel = 0;
    bool wifiConnected = false;

    SysObject wifi;
    QTimer* uiTimer;
    InputManager inputManager;
    Power* powerApi;
};