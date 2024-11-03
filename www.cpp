

#include "main.h"

extern WiFiClientSecure client; // Create a WiFiClientSecure object
extern WiFiClient wifi;
extern Stats stats;
extern Current_time now;
extern Octopuss cost_array[71]; // store all cost data for next day or 2

extern uint8_t log_data; // flags to say if data should be loged
extern hw_timer_t *clk_timer;
int64_t get_time_offset_in_sec = 0; //used in this file only to set clock

////////////////////////////////////////////////////////////////////
// alternative to delay which helps keep the clock updated
///////////////////////////////////////////////////////////////////
void tdelay(int del)
{
  getTime();
  delay(del);
}

////////////////////////////////////////////////////////////////
//
// gets current time from hw timer that counts to 1 minute
// must be called at least once / minute to keep time
///////////////////////////////////////////////////////////////'
void getTime(void) 
{
static uint64_t count;
static int last=0;
count = timerRead(clk_timer);
 timerAlarmEnable(clk_timer); //re Enable here so it can not latch up by missing this
now.sec = ((int)(count/1000000)) - get_time_offset_in_sec;  // correct for offset
if(now.sec < 0 || now.sec >59)
  now.sec = 60 - now.sec;
now.sec = abs(now.sec);


if(now.sec < last)
  { 
    timerAlarmEnable(clk_timer); //re Enable
    if(now.min < 59)
      now.min++;
    else
    {
      now.min = 0;
      if(now.hour < 23)
       now.hour++;
      else
      {
        now.hour = 0;
        now.date++; // as this is updated from the web site occasinaly it can correct for end of month
      }
    } // end else  
  }
 last = now.sec;
 now.sec_from_midnight = (now.hour*3600) + (now.min * 60) + now.sec;    
}


// ///////////////////////////////////////////////////////////////////
// Connects to Octopus api and gets next days electricity prices
// fills the array with data returned
// Will try up to 5 times and returns 200 for sucsese
// ///////////////////////////////////////////////////////////////////
int get_web_data(String apiURL,char name)
 {
  HTTPClient http;
  int attempts = 0;
  
  int code;
  
  do{
    Serial.println("Connecting to the HTTP server....");
   // std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
   // client->setInsecure();    

    http.setTimeout(60000);
    http.useHTTP10(true); // needed for stream

    if (http.begin( apiURL)) 
    {
        Serial.println("Connected");   
      code = http.GET();
      Serial.printf("HTTP Code [%d]", code);
      if (code > 0) {
        if (code == HTTP_CODE_OK || code == HTTP_CODE_MOVED_PERMANENTLY)
          {
            switch(name)
              {
                case 'o': do_octopuss_web(http);  break;
                case 't': do_time_web(http);      break;
                case 'w': do_wether_web(http);    break;
              }                
          http.end();  
          return(code);  
          }
    }
      else 
        {
        Serial.printf("[HTTP]  GET... failed, error: %s", http.errorToString(code).c_str()); 
        http.end();      
        }    
    }
    attempts++;
    tdelay(4000);
  }while(attempts < 5);
 return (code); // 200 if ok
}

///////////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////////////
void do_wether_web(HTTPClient &http)
{
  int x;
    stats.yesterdays_outside_min_temp = stats.outside_min_temp; 
    stats.outside_min_temp = stats.tomorow_outside_min_temp;
    String temp = getValue(http, "apparent_temperature_max", 1, 20); // get rid of first one we want the second   
    temp = getValue(http, "apparent_temperature_max", 3, 10);    
    x=temp.indexOf(',');
    stats.tomorow_outside_max_temp = temp.substring(x+1,x+5).toInt();

    temp = getValue(http,"apparent_temperature_min", 3, 10); 
    Serial.println(temp);
    x=temp.indexOf(',');
    stats.tomorow_outside_min_temp = temp.substring(x+1,x+5).toInt();

    temp = getValue(http,"sunshine_duration", 3, 15); 
    Serial.println(temp);
    x=temp.indexOf(',');
    stats.today_solar_energy = stats.tomorow_solar_energy; // remember last value
    stats.tomorow_solar_energy = temp.substring(x+1,x+5).toInt();
   
   
    Serial.println("temp tomorow is "+String(stats.tomorow_solar_energy));
}

 


//{"timezone":"UTC","formatted":"03.11.2024 11:09","timestamp":1730632166223,"weekDay":7,"day":3,"month":11,"year":2024,"hour":11,"minute":9}
void  do_time_web(HTTPClient &http)
{
    String temp = getValue(http, "formatted", 3, 23);  // gets data 10 char  
      Serial.println("Get time from web ");  
          Serial.println(temp);  
          now.date = temp.substring(0,2).toInt();           
          now.hour = temp.substring(11,13).toInt(); 
          now.min = temp.substring(14,16).toInt(); 
          now.sec = 0;// temp.substring(19,21).toInt(); 
          // counter has to be syncronised to seconds 
        //  timerWrite(clk_timer, now.sec * 1000000 ); //set Hw counter to nearest second appears to get stuck as reset value
          get_time_offset_in_sec = (timerRead(clk_timer)/1000000) - now.sec; // to provent minute incromenting on next get_time call        
        
          http.end();  
          getTime(); // updates struct
}



void do_octopuss_web(HTTPClient &http)
{
  void *p = &cost_array[0]; // pointer to struct
  uint8_t x;
   Serial.println("GET OK for Octopuss");           
          memset(p,0,sizeof(cost_array)); // clear old values out
          for(x=0;x<70;x++)  // warning 100 is expected to exist
          {
            String temp = getValue(http, "value_inc_vat", 1, 6);  // gets data 1 record at a time
           //  Serial.println(temp);
            cost_array[x].cost =  temp.substring(1,5).toFloat();    
            temp = getValue(http, "valid_from", 1, 20);  
            cost_array[x].date = temp.substring(10,12).toInt(); // was 10 12
            cost_array[x].hour = temp.substring(13,15).toInt(); 
            cost_array[x].min = temp.substring(16,18).toInt();  
            cost_array[x].sec_from_midnight = ( cost_array[x].hour*3600) + ( cost_array[x].min * 60); 
          }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// surch for string "key" once found move on skip then return "get" number of char
String getValue(HTTPClient &http, String key, int skip, int get) 
{
  bool found = false, look = false;
  int ind = 0;
  String ret_str = "";

  int len = http.getSize();
  char char_buff[1];
  WiFiClient * stream = http.getStreamPtr();
  while (http.connected() && (len > 0 || len == -1)) 
  {
    size_t size = stream->available();
    if (size) {
      int c = stream->readBytes(char_buff, ((size > sizeof(char_buff)) ? sizeof(char_buff) : size));
      if (len > 0)
        len -= c;
      if (found) {
        if (skip == 0) {
          ret_str += char_buff[0];
          get --;
        } else
          skip --;
        if (get <= 0)
          break;
      }
      else if ((!look) && (char_buff[0] == key[0])) {
        look = true;
        ind = 1;
      } else if (look && (char_buff[0] == key[ind])) {
        ind ++;
        if (ind == key.length()) found = true;
      } else if (look && (char_buff[0] != key[ind])) {
        ind = 0;
        look = false;
      }
    }
  }
  return(ret_str);
}


//----------------------------------------Host & httpsPort
const char* host = "script.google.com";
const int httpsPort = 443;
//----------------------------------------




// ////////////////////////////////////////////////////////////////////////////
// Subroutine for sending data to Google Sheets
//
// ///////////////////////////////////////////////////////////////////////////
void send_ss_day_data(int today_solar_energy, int pv_day, int soc, int load_day, int forcast_power_rec)
{
  char url[200];
  char buf2[300];
  char GAS_ID[] = "AKfycbzPwd4ebSzdXzDMBBD0LWXzRuS23YSVQqJseCi2Holgk1fiKd3t6ShqMLvnLYtRMGZizw"; //deployment ID for day spreadsheet
  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) 
  {
    Serial.println("Connecting to script.google.com failed");
    return;
  }
  //----------------------------------------

  //----------------------------------------Processing data and sending data

  sprintf(url,"https://script.google.com/macros/s/%s/exec?today_solar_energy=%d&pv_day=%d&soc=%d&load_day=%d&forcast_power_rec=%d",GAS_ID,today_solar_energy, pv_day, soc, load_day, forcast_power_rec);

  Serial.println(url);
  sprintf(buf2,"GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: BuildFailureDetectorESP8266\r\nConnection: close\r\n\r\n",url,host);
  client.print(buf2);
 
  Serial.println("request sent");
  //----------------------------------------

 while (client.connected()) // once all data is read the web site will close the connection
    {
      if (client.available())
      {
        client.readStringUntil('\n');
       // String line = client.readStringUntil('\n');
      //  Serial.println(line);
      }
    }
    client.stop();
    Serial.println("\n[Disconnected]");

 
} 




// ////////////////////////////////////////////////////////////////////////////
// Subroutine for sending data to Google Sheets call every 30 minutes
// 
// 
// ///////////////////////////////////////////////////////////////////////////
void send_ss_hh_data(int plan, int soc, float periods_of_charge, float total_charging_cost)
{
  char url[200];
  char buf2[300];
  char buf3[100];
  
// Google spreadsheet script ID
  char GAS_ID[]  = "AKfycbysbNjJkuHZJQxMuLO2CdK_KskSUS7fwbW116NWJLTlsLhVDq9C6Zg7YSE_4B61eFmgQQ";// v4 of 1/2 hour spreed sheet

  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) 
  {
    Serial.println("Connecting to script.google.com failed");
    return;
  }
  //----------------------------------------

  //----------------------------------------Processing data and sending data
 dtostrf(periods_of_charge ,4,2, buf2);
dtostrf(total_charging_cost ,4,2, buf3);
  sprintf(url,"https://script.google.com/macros/s/%s/exec?plan=%d&soc=%d&periods_of_charge=%s&total_charging_cost=%s",GAS_ID,plan, soc, buf2, buf3);

 // Serial.println(url);
  sprintf(buf2,"GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: BuildFailureDetectorESP8266\r\nConnection: close\r\n\r\n",url,host);
  client.print(buf2);
 
  Serial.println("request sent");
  //----------------------------------------
while (client.connected()) // once all data is read the web site will close the connection
    {
      if (client.available())
      {
        client.readStringUntil('\n');
       // String line = client.readStringUntil('\n');
      //  Serial.println(line);
      }
    }
    client.stop();
    Serial.println("\n[Disconnected]");
  
 
} 



