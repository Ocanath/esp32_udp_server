#ifndef NVS_H
#define NVS_H
#include "Preferences.h"

#define WIFI_MAX_SSID_LEN   32    //SSID max length is 32 characters
#define WIFI_MAX_PWD_LEN    63    //WPA2-PSK key length limit is 63 characters


typedef struct nvs_settings_t
{
  char ssid[WIFI_MAX_SSID_LEN];
  char password[WIFI_MAX_PWD_LEN];
  int port; //udp server forwarding port
  int reply_offset;	//offset applied to remote port for splitting send and receieve ports. default 0
  int baud;
  uint8_t nbytes_fchk;  //setting can be 0->default 4, 1, 2, or 4
  int nwords_expected; //must be 1 or more, 32bit words
}nvs_settings_t;

extern Preferences preferences;
extern nvs_settings_t gl_prefs;


void init_prefs(Preferences * p, nvs_settings_t * s);

#endif