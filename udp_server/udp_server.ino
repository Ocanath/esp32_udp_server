#include "WiFi.h"
#include "WiFiUdp.h"


#define IPV4_ADDR_ANY   0x00000000UL

enum {PERIOD_CONNECTED = 150, PERIOD_DISCONNECTED = 3000};


#define CHAR_BUF_SIZE 256
uint8_t gl_char_buf[256] = {0};
int gl_charbuf_loc = 0;

void parse_console_input(void)
{
    int rv = Serial.read();
    if(rv != -1)
    {
      uint8_t rchar = (uint8_t)rv;
      if(gl_charbuf_loc < CHAR_BUF_SIZE)
      {
        gl_char_buf[gl_charbuf_loc++] = rchar;
      }
      else
      {
        gl_charbuf_loc = 0;
      }

      //Serial.write((char)rv);
      Serial.printf("0x%0.2X",rchar);
    }
}

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
  udp.begin(server_address,11054);


  uint32_t blink_ts = 0;
  uint32_t blink_period = PERIOD_DISCONNECTED;
  uint8_t led_mode = 0;


  uint8_t udp_pkt_buf[256] = {0};
  while(1)
  {

    int len = udp.read(udp_pkt_buf,255);
    if(len > 0)
    {
      udp_pkt_buf[len] = 0; //clear last element to null term string before writing
      Serial.write("Received: ");
      Serial.write((const char *)udp_pkt_buf);
      Serial.write("\r\n");
      for(int i = 0; i < len; i++)
        udp_pkt_buf[i] = 0;
    }

    parse_console_input();

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
