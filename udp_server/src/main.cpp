#include "WiFi.h"
#include "WiFiUdp.h"
#include "parse_console.h"
#include "nvs.h"
#include "checksum.h"
#include "circ_scan.h"
#include "PPP.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define RELAY_PIN 25
#define SWITCH_PIN 26


/*
HARDWARE CONFIG:
Setport: the port we use (for office, 4593)
setssid and setpwd for office
setbaud 115200
setname to whatever
*/

#define IPV4_ADDR_ANY   0x00000000UL

enum {PERIOD_CONNECTED = 50, PERIOD_DISCONNECTED = 3000};

WiFiUDP udp;


void setup() {

  /*Do a power on blink pattern*/
  pinMode(2,OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
  digitalWrite(RELAY_PIN, HIGH);
  for(int i = 0; i < 4; i++)
  {
    digitalWrite(2,HIGH);
    delay(50);
    digitalWrite(2,LOW);
    delay(50);
  }
  init_prefs(&preferences, &gl_prefs);

  Serial.begin(460800);
  if(gl_prefs.baud != 0)
    Serial2.begin(gl_prefs.baud);
  if(gl_prefs.nwords_expected == 0) //quick & dirty kludge for init case of this parameter
    gl_prefs.nwords_expected = 1;
  
  int connected = 0;
  for(int attempts = 0; attempts < 1; attempts++)
  {
    Serial.printf("\r\n\r\n Trying \'%s\' \'%s\'\r\n",gl_prefs.ssid, gl_prefs.password);
    /*Begin wifi connection*/
    WiFi.mode(WIFI_STA);  
    WiFi.begin((const char *)gl_prefs.ssid, (const char *)gl_prefs.password);
    //connected = WiFi.waitForConnectResult();
    if (connected != WL_CONNECTED) {
      Serial.printf("Connection to network %s failed for an unknown reason\r\n", (const char *)gl_prefs.ssid);
    }

	Serial.print("Fuck Arduino\r\n");

	IPAddress server_address((uint32_t)IPV4_ADDR_ANY); //note: may want to change to our local IP, to support multiple devices on the network
	udp.begin(server_address, gl_prefs.port);

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

//lg_fifo_t gl_cb;
//uint32_t gl_cb_result[NUM_WORDS_FIFO];  //drawback of the circular buffer copy approach: must make it a double buffer. is pretty wasteful
#define UNSTUFFING_BUFFER_SIZE 256
#define PAYLOAD_BUFFER_SIZE ((UNSTUFFING_BUFFER_SIZE - 2)/2)  //max cap based on unstuffing buffer size
uint8_t gl_unstuffing_buffer[UNSTUFFING_BUFFER_SIZE] = {0};
uint8_t gl_pld_buffer[PAYLOAD_BUFFER_SIZE] = {0};


void set_target_lightstate(uint8_t state)
{
  if(gl_prefs.target_ip[0] == 0)
    return;

  IPAddress target_ip;
  if(target_ip.fromString( (const char *)gl_prefs.target_ip)  == false)
  {
    Serial.printf("Failed to parse ip\n");
    return;
  }
  Serial.printf("Targeting IP = %s\n", target_ip.toString());

  uint8_t noname = 0;
  if(gl_prefs.target_name[0] == 0)
    noname = 1;
  for(int attempts = 0; attempts < 5; attempts++)
  {
    //send to whatever the configuration port is, so it is fixed. Otherwise, a client not bound to this port could redirect light switch commands. 
    //ensure this device is bound to the same port as the target device.
    udp.beginPacket(target_ip, gl_prefs.port);  
    if(noname != 0)
    {
      if(state != 0)
      {
          int len = sprintf((char*)gl_pld_buffer, "lightson");
		udp.write(gl_pld_buffer, len);
		Serial.printf("%s\n", gl_pld_buffer);
      }
      else
      {
        int len = sprintf((char*)gl_pld_buffer, "lightsoff");
        udp.write(gl_pld_buffer, len);
        Serial.printf("%s\n", gl_pld_buffer);
      }
    }
    else
    {
      if(state != 0)
      {
          int len = sprintf((char*)gl_pld_buffer, "%s lightson", gl_prefs.target_name);
          udp.write(gl_pld_buffer, len);
          Serial.printf("%s\n", gl_pld_buffer);
      }
      else
      {
          int len = sprintf((char*)gl_pld_buffer, "%s lightsoff", gl_prefs.target_name);
          udp.write(gl_pld_buffer, len);
          Serial.printf("%s\n", gl_pld_buffer);
      }      
    }
    udp.endPacket();
  }
}




  uint32_t blink_ts = 0;
  uint32_t blink_period = PERIOD_DISCONNECTED;
  uint8_t led_mode = 1;

  uint8_t udp_pkt_buf[256] = {0};
  uint32_t packet_update_ts = 0;
  uint8_t activate_hose = 0;
  
  int radar_range = 0;
  uint8_t radar_acquisition = 0;
  uint8_t prev_radar_acquisition = 0;
  uint32_t bump_target_ts = 0;
  uint8_t pipe_radar_state = 0;
  uint8_t console_print_radar = 0;

  uint8_t prev_switch_state = 0;
  uint8_t relay_state = 1;
  int ppp_stuffing_bidx = 0;  //arg output/static variable for indexing into the stuffing buffer for ppp unpacking
  uint32_t switch_debounce_ts = 0;
  uint32_t checkforudpsave_ts = 0;


void loop() 
{  
    ArduinoOTA.handle();  //handle OTA updates!
    uint8_t udp_save_triggered = 0;
    int len = udp.parsePacket();
    if(len != 0)
    {
      int len = udp.read(udp_pkt_buf,255);
      // Serial2.write(udp_pkt_buf,len);
      
      /*Simple bkst reply to allow a client to confirm our IP. 
      Sends mac address as a unique identifier, to handle responses from multiple
      devices on network if there are multiple*/
      int cmp = -1;
      cmp = cmd_match((const char *)udp_pkt_buf,"marco");
      if(cmp > 0)
      {
        uint8_t query_response[11] = {0};
        query_response[0] = 'p';
        query_response[1] = 'o';
        query_response[2] = 'l';
        query_response[3] = 'o';
        query_response[4] = ' ';
        WiFi.macAddress((&query_response[5]));
        udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
        udp.write(query_response,11);
        udp.endPacket();
      }
      cmp = cmd_match((const char *)udp_pkt_buf,"activate_hose");
      if(cmp > 0)
      {
        activate_hose = 1;
      }
      cmp = cmd_match((const char *)udp_pkt_buf,"deactivate_hose");
      if(cmp > 0)
      {
        activate_hose = 0;
      }
      

      uint8_t match = 0;
      uint8_t name_match = 0;
      cmp = cmd_match((const char *)udp_pkt_buf, gl_prefs.our_name);
      if(cmp > 0)
      {
        if(udp_pkt_buf[cmp] == ' ')
        {
          name_match = 1;
          cmp++;  //skip the space
          int cpystart = 0;
          for(int i = cmp; i < sizeof(udp_pkt_buf); i++)
          {
            udp_pkt_buf[cpystart] = udp_pkt_buf[i];
            cpystart++;
          }
          for(int i = cpystart; i < sizeof(udp_pkt_buf); i++)
            udp_pkt_buf[i] = 0;
        }
      }
      
      //name must match to set ignore command
      if(name_match != 0)
      {
        cmp = cmd_match((const char*)udp_pkt_buf, "erase-name");
        if(cmp > 0)
        {
          for(int i = 0; i < sizeof(gl_prefs.target_name); i++)
            gl_prefs.target_name[i] = 0;
          match = 1;
          udp_save_triggered = 1;
        }
        cmp = cmd_match((const char*)udp_pkt_buf, "target-name ");
        if(cmp > 0)
        {
          for(int i = 0; i < sizeof(gl_prefs.target_name); i++)
            gl_prefs.target_name[i] = 0;
          int nameidx = 0;
          for(int i = cmp; i < sizeof(udp_pkt_buf) && i < sizeof(gl_prefs.target_name); i++)
          {
            gl_prefs.target_name[nameidx++] = udp_pkt_buf[i];
          }
          Serial.printf("Setting Target Name to %s\r\n",gl_prefs.target_name);
          match = 1;
          udp_save_triggered = 1;
        }
        cmp = cmd_match((const char*)udp_pkt_buf, "rtarget-name");
        if(cmp > 0)
        {
          int len = sprintf((char*)gl_pld_buffer, "Target Name=%s", gl_prefs.target_name);
          udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
          udp.write(gl_pld_buffer, len);
          udp.endPacket();
          match = 1;
        }

        cmp = cmd_match((const char*)udp_pkt_buf, "target-ip ");
        if(cmp > 0)
        {
          for(int i = 0; i < sizeof(gl_prefs.target_ip); i++)
            gl_prefs.target_ip[i] = 0;
          int nameidx = 0;
          for(int i = cmp; i < sizeof(udp_pkt_buf) && i < sizeof(gl_prefs.target_ip); i++)
          {
            gl_prefs.target_ip[nameidx++] = udp_pkt_buf[i];
          }
          Serial.printf("Setting Target IP to %s\r\n",gl_prefs.target_ip);
          match = 1;
          udp_save_triggered = 1;
        }
        cmp = cmd_match((const char*)udp_pkt_buf, "rtarget-ip");
        if(cmp > 0)
        {
          int len = sprintf((char*)gl_pld_buffer, "Target IP=%s", gl_prefs.target_ip);
          udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
          udp.write(gl_pld_buffer, len);
          udp.endPacket();
          match = 1;
        }


        cmp = cmd_match((const char*)udp_pkt_buf, "setignore");
        if(cmp > 0)
        {
          gl_prefs.ignore_general_cmd = 1;
          const char * msg = "ignore bkst on\n";
          Serial.printf("%s",msg);
          udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
          udp.write((uint8_t*)msg, strlen(msg));
          udp.endPacket();
          match = 1;
          udp_save_triggered = 1;
        }
        cmp = cmd_match((const char*)udp_pkt_buf, "clearignore");
        if(cmp > 0)
        {
          gl_prefs.ignore_general_cmd = 0;
          const char * msg = "ignore bkst off\n";
          Serial.printf("%s",msg);
          udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
          udp.write((uint8_t*)msg, strlen(msg));
          udp.endPacket();
          match = 1;
          udp_save_triggered = 1;
        }
        cmp = cmd_match((const char *)udp_pkt_buf,"piperadar");
        if(cmp > 0)
        {
          pipe_radar_state = 1;
        }
        cmp = cmd_match((const char *)udp_pkt_buf,"stopradar");
        if(cmp > 0)
        {
          pipe_radar_state = 0;
        }
      }
      if( (gl_prefs.ignore_general_cmd == 0 && name_match == 0) || name_match != 0)
      {
        cmp = cmd_match((const char*)udp_pkt_buf, "lightson");
        if(cmp > 0)
        {
          relay_state = 1;
          digitalWrite(RELAY_PIN, relay_state);
          match = 1;
          Serial.printf("lightson\n");
        }
        cmp = cmd_match((const char*)udp_pkt_buf, "lightsoff");
        if(cmp > 0)
        {
          relay_state = 0;
          digitalWrite(RELAY_PIN, relay_state);
          match = 1;
          Serial.printf("lightsoff\n");
        }
      }
      //always give stat and whoareyou responses
      cmp = cmd_match((const char*)udp_pkt_buf, "lightstat");
      if(cmp > 0)
      {
        uint8_t stat_response[] = {'l','i','g','h','t','s','o','f','f',0};
        if(relay_state == 1)
        {
          stat_response[sizeof(stat_response)-1] = 0;
          stat_response[sizeof(stat_response)-2] = 0;
          stat_response[sizeof(stat_response)-3] = 'n';
        }
        udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
        udp.write(stat_response,sizeof(stat_response));
        udp.endPacket();
        Serial.printf("lightstat\n");
        match = 1;
      }
      cmp = cmd_match((const char*)udp_pkt_buf, "whoareyou");
      if(cmp > 0)
      {
        int len = strlen(gl_prefs.our_name);
        udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
        udp.write((uint8_t*)gl_prefs.our_name, len);
        udp.endPacket();
        match = 1;
      }


      if(match == 0)
      {
        Serial.printf("err: %s unknown\n",udp_pkt_buf);
      }

      for(int i = 0; i < len; i++)
        udp_pkt_buf[i] = 0;
    }

    {
      uint32_t tick = millis();
      if((tick - switch_debounce_ts) > 100)
      {
        switch_debounce_ts = tick;
        int stat = digitalRead(SWITCH_PIN);
        if(stat != prev_switch_state)
        {
          if(stat == 0)
            Serial.printf("switch on\n");
          else
            Serial.printf("switch off\n");
          relay_state = ~stat & 1;
          digitalWrite(RELAY_PIN, relay_state);
        }
        prev_switch_state = (uint8_t)stat;
      }
    }
    /*The following pseudocode should be used for offloading
    32bit fletcher's checksum masked uart packets over UDP, without
    having to write my own baremetal UART for the ESP32. TODO: determine
    whether this will work. consider using a loopback approach with logic
    analyzer for ease of use*/
    uint8_t serial_pkt_sent = 0;
    while(Serial2.available())
    {
      uint8_t new_byte = Serial2.read();
      if(ppp_stuffing_bidx < sizeof(gl_unstuffing_buffer))
      {
        gl_unstuffing_buffer[ppp_stuffing_bidx++] = new_byte;
        // printf("%c",new_byte);
      }
      else
      {
        ppp_stuffing_bidx = 0;
      }


      if(ppp_stuffing_bidx >=2)
      {
        if(gl_unstuffing_buffer[ppp_stuffing_bidx-1] == '\n' && gl_unstuffing_buffer[ppp_stuffing_bidx-2] == '\r')
        {
         
          //erase the remaining contents of the buffer. ppp_stuffing_bidx is protected because of the above parsing codes
          for(int i = ppp_stuffing_bidx; i < sizeof(gl_unstuffing_buffer); i++)
          {
            gl_unstuffing_buffer[i] = 0;
          }

          int cmp = -1;
          cmp = cmd_match((const char *)gl_unstuffing_buffer,"Range ");
          if(cmp > 0)
          {
            uint8_t postarg_buf[32] = {0};
            int postarg_idx = 0;
            for(int i = cmp; (i < ppp_stuffing_bidx - 2); i++)
            {
              postarg_buf[postarg_idx++] = gl_unstuffing_buffer[i];
            }

            char * tmp;
            radar_range = strtol((const char *)postarg_buf, &tmp, 10);
            if(console_print_radar != 0)
              Serial.printf("target range = %d\r\n", radar_range);
          }
          cmp = cmd_match((const char *)gl_unstuffing_buffer,"ON");
          if(cmp > 0)
          {
            radar_acquisition = 1;
            if(console_print_radar != 0)
              Serial.printf("Target Acquired\r\n");
          }
          cmp = cmd_match((const char *)gl_unstuffing_buffer,"OFF");
          if(cmp > 0)
          {
            radar_acquisition = 0;
            if(console_print_radar != 0)
              Serial.printf("Target Lost\r\n");
          }

          if(pipe_radar_state != 0)
          {
            int len = sprintf((char*)gl_pld_buffer, "acq=%d, range=%d\n", radar_acquisition, radar_range);
            udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
            udp.write(gl_pld_buffer, len);
            udp.endPacket();
          }

          ppp_stuffing_bidx = 0;



          uint32_t tick = millis();
          if( (radar_acquisition != prev_radar_acquisition) || ((tick - bump_target_ts) > 10000) )
          {
            bump_target_ts = tick;
            set_target_lightstate(radar_acquisition);
            prev_radar_acquisition = radar_acquisition;
          }
        

        }

      }

      //  int pld_len = parse_PPP_stream(new_byte, gl_pld_buffer, PAYLOAD_BUFFER_SIZE, gl_unstuffing_buffer, UNSTUFFING_BUFFER_SIZE, &ppp_stuffing_bidx);
      //  if(pld_len != 0)
      //  {
      //     udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
      //     udp.write((uint8_t*)gl_pld_buffer, pld_len);
      //     udp.endPacket();      
      //     serial_pkt_sent = 1;
      //  }
    }

    if(activate_hose != 0)
    {
      /*Point the hose at the person who looked at us*/
      uint32_t tick = millis();
      if(tick - packet_update_ts > 20)
      {
        packet_update_ts = tick;
        if(serial_pkt_sent == 0)
        {
          udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
          udp.write((uint8_t*)"WAZZUP",6);
          udp.endPacket();          
        }
      }
    }
    get_console_lines();

    {
      uint32_t tick = millis();
      if((tick - checkforudpsave_ts) > 1000 && udp_save_triggered != 0)
      {
        udp_save_triggered = 0;
        checkforudpsave_ts = tick;
        int nb = preferences.putBytes("settings", &gl_prefs, sizeof(nvs_settings_t));
        Serial.printf("Saved %d bytes\r\n", nb);
        const char * msg = "saved data";
        udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
        udp.write((uint8_t*)msg, strlen(msg));
        udp.endPacket();
      }
    }

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
        Serial.printf("Server Response Offset: %d\r\n", gl_prefs.reply_offset);
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

      cmp = cmd_match((const char *)gl_console_cmd.buf,"setname ");
      if(cmp > 0)
      {
        match = 1;
        const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        /*Set the ssid*/
        for(int i = 0; i < NAME_SIZE; i++)
        {
          gl_prefs.our_name[i] = '\0';
        }
        for(int i = 0; arg[i] != '\0'; i++)
        {
          if(arg[i] != '\r' && arg[i] != '\n')  //copy non carriage return characters
          {
            gl_prefs.our_name[i] = arg[i];
          }
        }
        Serial.printf("Changing name to: %s\r\n", gl_prefs.our_name);
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
	  
      /*Parse set port command*/
      cmp = cmd_match((const char *)gl_console_cmd.buf,"setTXoff ");
      if(cmp > 0)
      {
        match = 1;
    		const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
        char * tmp;
        int offset = strtol(arg, &tmp, 10);
		    Serial.printf("Setting port offset to: %d\r\n",offset);
    		gl_prefs.reply_offset = offset;
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

      cmp = cmd_match((const char *)gl_console_cmd.buf, "plotradar");
      if(cmp > 0)
      {
        match = 1;
        console_print_radar = 1;
      }
      cmp = cmd_match((const char *)gl_console_cmd.buf, "stopradar");
      if(cmp > 0)
      {
        match = 1;
        console_print_radar = 0;
      }

      cmp = cmd_match((const char*)gl_console_cmd.buf, "target-on");
      if(cmp > 0)
      {
        match = 1;
        set_target_lightstate(1);
      }
      cmp = cmd_match((const char*)gl_console_cmd.buf, "target-off");
      if(cmp > 0)
      {
        match = 1;
        set_target_lightstate(0);
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
