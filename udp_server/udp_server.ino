#include "WiFi.h"
#include "WiFiUdp.h"
#include "parse_console.h"
#include "nvs.h"
#include "checksum.h"
#include "circ_scan.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define IPV4_ADDR_ANY   0x00000000UL

enum {PERIOD_CONNECTED = 50, PERIOD_DISCONNECTED = 3000};

WiFiUDP udp;

void setup() {

  /*Do a power on blink pattern*/
  pinMode(2,OUTPUT);
  for(int i = 0; i < 4; i++)
  {
    digitalWrite(2,HIGH);
    delay(50);
    digitalWrite(2,LOW);
    delay(50);
  }
  init_prefs(&preferences,&gl_prefs);

  Serial.begin(460800);
  if(gl_prefs.baud != 0)
    Serial1.begin(gl_prefs.baud);
  if(gl_prefs.nwords_expected == 0) //quick & dirty kludge for init case of this parameter
    gl_prefs.nwords_expected = 1;
  
  int connected = 0;
  for(int attempts = 0; attempts < 10; attempts++)
  {
    Serial.printf("\r\n\r\n Trying \'%s\' \'%s\'\r\n",gl_prefs.ssid, gl_prefs.password);
    /*Begin wifi connection*/
    WiFi.mode(WIFI_STA);  
    WiFi.begin((const char *)gl_prefs.ssid, (const char *)gl_prefs.password);
    connected = WiFi.waitForConnectResult();
    while (connected != WL_CONNECTED) {
      Serial.printf("Connection to network %s failed for an unknown reason\r\n", (const char *)gl_prefs.ssid);
    }
  }
  if(connected == 0)
  {
    while(1)
    {
      uint8_t toggle = 0;
      for(uint32_t delay_ms = 2000; delay_ms >= 50; delay_ms-=50)
      {
        digitalWrite(2,toggle);
        delay(delay_ms);
        toggle = (~toggle) & 1;
      }
    }
  }


  /*
  Arduino OTA setup
  */
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

}

int cmd_match(const char * in, const char * cmd)
{
  int i = 0;
  for(i = 0; cmd[i] != '\0'; i++)
  {
    if(in[i] == '\0')
      return -1;
    if(in[i] != cmd[i])
      return -1;   
  }
  return i;
}

lg_fifo_t gl_cb;
uint32_t gl_cb_result[NUM_WORDS_FIFO];  //drawback of the circular buffer copy approach: must make it a double buffer. is pretty wasteful

void loop() {  

  Serial.print("Fuck Arduino\r\n");

  IPAddress server_address((uint32_t)IPV4_ADDR_ANY); //note: may want to change to our local IP, to support multiple devices on the network
  udp.begin(server_address, gl_prefs.port);


  uint32_t blink_ts = 0;
  uint32_t blink_period = PERIOD_DISCONNECTED;
  uint8_t led_mode = 1;

  uint8_t udp_pkt_buf[256] = {0};
  while(1)
  {
    ArduinoOTA.handle();  //handle OTA updates!

    int len = udp.parsePacket();
    if(len != 0)
    {
      int len = udp.read(udp_pkt_buf,255);
      Serial1.write(udp_pkt_buf,len);
      
      for(int i = 0; i < len; i++)
        udp_pkt_buf[i] = 0;
    }
    /*The following pseudocode should be used for offloading
    32bit fletcher's checksum masked uart packets over UDP, without
    having to write my own baremetal UART for the ESP32. TODO: determine
    whether this will work. consider using a loopback approach with logic
    analyzer for ease of use*/
    while(Serial1.available())
    {
       uint8_t d = Serial1.read();
       add_circ_buffer_element(d, &gl_cb);
       int len;
       if(scan_lg_fifo_fchk32(&gl_cb, gl_prefs.nwords_expected, gl_cb_result, &len) == 1)
       {
          // Serial.printf("Found: 0x");
          // for(int i = 0; i < len; i++)
          //   Serial.printf("%0.2X",gl_cb_result[i]);
          // Serial.printf("\r\n");
          udp.beginPacket(udp.remoteIP(), udp.remotePort());
          udp.write((uint8_t*)gl_cb_result,len*sizeof(uint32_t));
          udp.endPacket();          
       }
    }
 
    get_console_lines();

    /*Long, hideous, kludged the fuck out command line parser. Don't care, this fw has well defined functionality requirements
     and it just has to work, so dev speed trumps maintainability */
    if(gl_console_cmd.parsed == 0)
    {
      uint8_t match = 0;
      uint8_t save = 0;
      int cmp = -1;

      /*Parse the command to get the local IP and connection status*/
      cmp = strcmp((const char *)gl_console_cmd.buf,"ipconfig\r");
      if(cmp == 0)
      {
        match = 1;
        if(WiFi.status() == WL_CONNECTED)
        {
          Serial.printf("Connected to: %s\r\n", gl_prefs.ssid);
        }
        else
        {
          Serial.printf("Not connected to: %s\r\n", gl_prefs.ssid);
        }
        Serial.printf("UDP server on port: %d\r\n", gl_prefs.port);
        Serial.printf("IP address is: %s\r\n", WiFi.localIP().toString().c_str());
      }

      /*Parse the command to get UDP server access port*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"udpconfig\r");
      if(cmp > 0)
      {
        match = 1;
        Serial.printf("UDP server on port: %d\r\n", gl_prefs.port);
      }
      
      /*Parse ssid command*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"setssid ");
      if(cmp > 0)
      {
        match = 1;
        const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        /*Set the ssid*/
        for(int i = 0; i < WIFI_MAX_SSID_LEN; i++)
        {
          gl_prefs.ssid[i] = '\0';
        }
        for(int i = 0; arg[i] != '\0'; i++)
        {
          if(arg[i] != '\r' && arg[i] != '\n')  //copy non carriage return characters
          {
            gl_prefs.ssid[i] = arg[i];
          }
        }
        Serial.printf("Changing ssid to: %s\r\n", gl_prefs.ssid);
        save = 1;
      }

      /*Parse password command*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"setpwd ");
      if(cmp > 0)
      {
        match = 1;
        const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        /*Set the password*/
        for(int i = 0; i < WIFI_MAX_PWD_LEN; i++)
        {
          gl_prefs.password[i] = '\0';
        }
        for(int i = 0; arg[i] != '\0'; i++)
        {
          if(arg[i] != '\r' && arg[i] != '\n')
          {
            gl_prefs.password[i] = arg[i];
          }
        }
        Serial.printf("Changing pwd to: %s\r\n",gl_prefs.password);
        save = 1;
      }

      /*Parse set port command*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"setport ");
      if(cmp > 0)
      {
        match = 1;
        const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        char * tmp;
        int port = strtol(arg, &tmp, 10);
        Serial.printf("Changing port to: %d\r\n",port);
        /*Set the port*/
        gl_prefs.port = port;
        save = 1;
      }

      /*Parse read ssid and pwd command*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"readcred");
      if(cmp > 0)
      {
        match = 1;
        Serial.printf("SSID: \'");
        for(int i = 0; gl_prefs.ssid[i] != 0; i++)
        {
          char c = gl_prefs.ssid[i];
          if(c >= 0x1f && c <= 0x7E)
          {
            Serial.printf("%c",c);
          }
          else
          {
            Serial.printf("%0.2X",c);
          }
        }
        Serial.printf("\'\r\n");

        Serial.printf("Password: \'");
        for(int i = 0; gl_prefs.password[i] != 0; i++)
        {
          char c = gl_prefs.password[i];
          if(c >= 0x1f && c <= 0x7E)
          {
            Serial.printf("%c",c);
          }
          else
          {
            Serial.printf("%0.2X",c);
          }
        }
        Serial.printf("\'\r\n");

        
      }

      /*Parse command to change the UART UDP forward baud rate*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"setbaud ");
      if(cmp > 0)
      {
        match = 1;
        const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        char * tmp;
        int baud = strtol(arg, &tmp, 10);
        Serial.printf("Changing baud to: %d\r\n",baud);
        /*Set the baud and reinitalize the slave UART*/
        gl_prefs.baud = baud;
        save = 1;
      }
      
      /*Parse command to change the UART expected packet size. DO NOT INCLUDE CHECKSUM AS A WORD! 
      I.e. if you have 4 words + 1 checksum for 5 total words, setrsize = 4 for proper readings.*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"setrsize ");
      if(cmp > 0)
      {
        match = 1;
        const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        char * tmp;
        int size = strtol(arg, &tmp, 10);
        Serial.printf("Changing rsize to: %d\r\n",size);
        /*Set the baud and reinitalize the slave UART*/
        gl_prefs.nwords_expected = size;
        save = 1;
      }


      /*Parse command to report current baud setting*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"readbaud\r");
      if(cmp > 0)
      {
        match = 1;
        Serial.printf("Baud is: %d\r\n", gl_prefs.baud);
      }


      /*Parse command to report current baud setting*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"readrsize\r");
      if(cmp > 0)
      {
        match = 1;
        Serial.printf("Readsize: %d\r\n", gl_prefs.nwords_expected);
      }

      /*Parse connect command*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"reconnect\r");
      if(cmp > 0)
      {
        match = 1;
        Serial.printf("restarting wifi connection...\r\n");
        /*Try to connect using modified ssid and password. for convenience, as a restart will fulfil the same functionality*/
        WiFi.disconnect();
        WiFi.begin((const char *)gl_prefs.ssid,(const char *)gl_prefs.password);
        udp.begin(server_address, gl_prefs.port);
      }

      cmp = cmd_match((const char *)gl_console_cmd.buf,"restart\r");
      if(cmp > 0)
      {
        Serial.printf("restarting chip...\r\n");
        ESP.restart();
      }


      if(match == 0)
      {
        Serial.printf("Failed to parse: %s\r\n", gl_console_cmd.buf);
      }
      if(save != 0)
      {
        int nb = preferences.putBytes("settings", &gl_prefs, sizeof(nvs_settings_t));
        Serial.printf("Saved %d bytes\r\n", nb);
      }

      for(int i = 0; i < BUFFER_SIZE; i++)
      {
        gl_console_cmd.buf[i] = 0; 
      }
      gl_console_cmd.parsed = 1;
    }


    if(WiFi.status() != WL_CONNECTED)
    {
      blink_period = PERIOD_DISCONNECTED;
    }
    else
    {
      blink_period = PERIOD_CONNECTED;
    }



    if(millis() - blink_ts > blink_period)
    {
      blink_ts = millis();
      digitalWrite(2, led_mode);
      led_mode = (~led_mode) & 1;
      if(WiFi.status() != WL_CONNECTED)
      {
        //WiFi.reconnect();
        WiFi.disconnect();
        WiFi.begin((const char *)gl_prefs.ssid,(const char *)gl_prefs.password);
        udp.begin(server_address, gl_prefs.port);

      }
    }
  }  

}
