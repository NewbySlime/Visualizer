#ifndef WIFI_HEADER
#define WIFI_HEADER


enum WifiState{
  Connected,
  CantConnect,
  Disconnected,
  Waiting
};

typedef void (*WifiStateCallback)(WifiState state);

void wifi_reconnect();
void wifi_update(void *obj);
void wifi_start();
void wifi_change(const char *ssid, const char *pass);
void wifi_setcallback(WifiStateCallback cb);
void wifi_disconnect();

WifiState wifi_currentstate();

#endif