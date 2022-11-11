#include "WiFi.h"
#include "WiFiUdp.h"
#include "Preferences.h"
#include "parse_console.h"

#define IPV4_ADDR_ANY   0x00000000UL

enum {PERIOD_CONNECTED = 50, PERIOD_DISCONNECTED = 3000};

WiFiUDP udp;

void setup() {
  //fuck you
}

void loop() {  
  /*Do a power on blink pattern*/
  pinMode(2,OUTPUT);
  for(int i = 0; i < 4; i++)
  {
    digitalWrite(2,HIGH);
    delay(50);
    digitalWrite(2,LOW);
    delay(50);
  }

  /*Begin wifi connection*/
  WiFi.mode(WIFI_STA);
  const char * ssid = "PSYONIC Guest";
  const char * password = "pgsuyeosnt1c";
  WiFi.begin(ssid,password);

  Serial.begin(460800);

  IPAddress server_address((uint32_t)IPV4_ADDR_ANY); //
  udp.begin(server_address, 3425);


  uint32_t blink_ts = 0;
  uint32_t blink_period = PERIOD_DISCONNECTED;
  uint8_t led_mode = 0;

  uint8_t udp_pkt_buf[256] = {0};
  while(1)
  {

    int len = udp.parsePacket();
    if(len != 0)
    {
      int len = udp.read(udp_pkt_buf,255);
      udp_pkt_buf[len] = 0; //clear last element to null term string before writing
      Serial.write("Received: ");
      Serial.write((const char *)udp_pkt_buf);
      Serial.write("\r\n");

      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.printf("received: ");
      udp.printf((const char *)udp_pkt_buf);
      udp.printf("\r\n");
      udp.endPacket();

      for(int i = 0; i < len; i++)
        udp_pkt_buf[i] = 0;
    }

    get_console_lines();


    if(gl_console_cmd.parsed == 0)
    {
      if(strcmp((const char *)gl_console_cmd.buf,"ipconfig\r") == 0)
      {
        Serial.printf("IP address is: 0x%0.8X\r\n", WiFi.localIP());
      }
      else
      {
        Serial.printf("Failed to parse: %s\r\n", gl_console_cmd.buf);
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
        WiFi.reconnect();
      }
    }
  }  

}
