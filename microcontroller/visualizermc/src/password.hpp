#ifndef PASSWORD_HEADER
#define PASSWORD_HEADER

#include "utility"
#include "string"

#define MAX_SSID_STRLEN 31
#define MAX_PASSWORD_STRLEN 63


// this will be put in a different source code to keep a secret
const char *PWD_getDefaultSSID();
const char *PWD_getDefaultPassword();


void PWD_init();

size_t PWD_alternateCount();
size_t PWD_maxAlternates();


std::pair<const char*, const char*> PWD_getAuth(int idx);
bool PWD_setAuth(int idx, std::string ssid, std::string pwd);

bool PWD_removeAuth(int idx);
bool PWD_addAuth(std::string ssid, std::string pwd);

#endif