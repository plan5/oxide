#ifndef DBUSSETTINGS_H
#define DBUSSETTINGS_H

#define OXIDE_DBUS_CONFIG_PATH "/etc/dbus-1/system.d/codes.eeems.oxide.conf"
#define OXIDE_DBUS_CONFIG_LOCAL_PATH ":/codes.eeems.oxide.conf"
#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

#define OXIDE_GENERAL_INTERFACE OXIDE_SERVICE ".General"
#define OXIDE_POWER_INTERFACE OXIDE_SERVICE ".Power"
#define OXIDE_WIFI_INTERFACE OXIDE_SERVICE ".Wifi"

#endif // DBUSSETTINGS_H
