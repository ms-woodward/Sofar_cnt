//#include <dummy.h>
//#include <StreamUtils.h>
#include "main.h"


// The device name is used as the name displayed oy the wify hub.
const char* deviceName = "Sofar_cnt";
const char* version = "v1.0.0";
uint8_t log_data =0; // flags to say if data should be logged




/*****
Sofar_cnt is a control interface for Sofar solar and battery inverters.
It control the inverter to balence grid load and minimise cost of electrisity on the Octopus Adjile tarif. 

For read only mode, it will send status messages without the inverter needing to be in passive mode.  
It's designed to run on an ESP32 microcontroller with a TTL to RS485 module such as MAX485 or MAX3485.  
Designed to work with TTL modules without the DR and RE flow control pins.

Sofar/state
Which provides:

running_state  
grid_voltage  x10
grid_current  x100
grid_freq     x100
systemIO_power (AC side of inverter) 
battery_power  (DC side of inverter)
battery_voltage  x100
battery_current
batterySOC  
battery_temp  
battery_cycles  
grid_power  
consumption  
solarPV  
today_generation  
today_exported  
today_purchase  
today_consumption  
inverter_temp  
inverterHS_temp  
solarPVAmps
 
based on Sofar2mqtt design
c)Colin McGerty 2021 colin@mcgerty.co.uk
Major version 2.0 rewrite by Adam Hill sidepipeukatgmaildotcom
Thanks to Rich Platts for hybrid model code and testing.
calcCRC by angelo.compagnucci@gmail.com and jpmzometa@gmail.com
*****/
#if (! defined INVERTER_ME3000) && ! defined INVERTER_HYBRID
#error You must specify the inverter type.
#endif



// Wifi parameters.

const char* wifiName = WIFI_SSID;
WiFiClient wifi;
WiFiClientSecure client; // Create a WiFiClientSecure object
// web parameters
#include <PubSubClient.h>
const char* mqttClientID = deviceName;
PubSubClient mqtt(wifi);

SPIClass mySpi = SPIClass(VSPI); // touch screen spi
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);


unsigned int INVERTER_RUNNINGSTATE;

Octopuss cost_array[71]; // store all cost data for next day or 2

Stats stats;

hw_timer_t *clk_timer = NULL;

Current_time now;

struct mqtt_status_register
{
	uint16_t regnum;
	String    mqtt_name;
};

static struct mqtt_status_register  mqtt_status_reads[] =
{
	{ SOFAR_REG_RUNSTATE, "running_state" },
	{ SOFAR_REG_GRIDV, "grid_voltage" },
	{ SOFAR_REG_GRIDA, "grid_current" },
	{ SOFAR_REG_GRIDFREQ, "grid_freq" },
	{ SOFAR_REG_GRIDW, "grid_power" },
	{ SOFAR_REG_BATTW, "battery_power" },
	{ SOFAR_REG_BATTV, "battery_voltage" },
	{ SOFAR_REG_BATTA, "battery_current" },
	{ SOFAR_REG_SYSIOW, "systemIO_power" },
	{ SOFAR_REG_BATTSOC, "batterySOC" },
	{ SOFAR_REG_BATTTEMP, "battery_temp" },
	{ SOFAR_REG_BATTCYC, "battery_cycles" },
	{ SOFAR_REG_LOADW, "consumption" },
	{ SOFAR_REG_PVW, "solarPV" },
	{ SOFAR_REG_PVA, "solarPVAmps" },
	{ SOFAR_REG_PVDAY, "today_generation" },
#ifdef INVERTER_ME3000
	{ SOFAR_REG_EXPDAY, "today_exported" },
	{ SOFAR_REG_IMPDAY, "today_purchase" },
#elif defined INVERTER_HYBRID
	{ SOFAR_REG_PV1, "Solarpv1" },
	{ SOFAR_REG_PV2, "Solarpv2" },
#endif
	{ SOFAR_REG_LOADDAY, "today_consumption" },
	{ SOFAR_REG_INTTEMP, "inverter_temp" },
	{ SOFAR_REG_HSTEMP, "inverter_HStemp" },
};



// These timers are used in the main loop time in seconds.
#define HEARTBEAT_INTERVAL 9
#define RUNSTATE_INTERVAL 5
#define SEND_INTERVAL 10
#define BATTERYSAVE_INTERVAL 3



Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, -1, TFT_MISO);
//Adafruit_ILI9341 tft = Adafruit_ILI9341(D0, D4, D8, D5, -1, D3); // -1 for not fitted used ok for mini and 
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC,  -1);
// Update the OLED. Use NULL for no change 

/*
 the message containing the response from
the inverter, which has a result code in the lower byte and status in the upper byte.

The result code will be 0 for success, 1 means "Invalid Work Mode" ( which possibly means
the inverter isn't in passive mode, ) and 3 means "Inverter busy." 2 and 4 are data errors
which shouldn't happen unless there's a cable issue or some such.

The status bits in the upper byte indicate the following:
Bit 0 - Charge enabled
Bit 1 - Discharge enabled
Bit 2 - Battery full, charge prohibited
Bit 3 - Battery flat, discharge prohibited

For example, a publish to Sofar2mqtt/set/charge will result in one on Sofar2mqtt/response/charge.
AND the message with 0xff to get the result code, which should be 0.

battery_save is a hybrid auto mode that will charge from excess solar but not discharge.*/
///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////
void implement_control(void) // implement strategy when required
{
  uint8_t x = now.array_now; // was found when planing happens every 1/2 hour
  uint loop=0;
  char buf[100];
//cost_array[x].plan = 1;
      sprintf(buf,"Implementing settings Plan %X x=%d",cost_array[x].plan & 0xF,x);
      Serial.println(buf);

      if(cost_array[x].plan & CHARGE != 0)
        {
          do{
            sendPassiveCmd(SOFAR_SLAVE_ID, SOFAR_FN_CHARGE, 	MAX_POWER, "Charging ");
            tdelay(3000);
            loop++;
          }while(INVERTER_RUNNINGSTATE != charging && loop < 16); //2
        }
      else 
        {
          do{
              sendPassiveCmd(SOFAR_SLAVE_ID, SOFAR_FN_AUTO, 0, "Auto      "); 
              tdelay(3000);
              loop++;
            }while(INVERTER_RUNNINGSTATE != waiting && loop < 16); //2
        } 
        
      if(cost_array[x].plan & HEAT != 0)
        digitalWrite(RELAY_PIN, HIGH); // turn on heating and maybe hot water
      else
        digitalWrite(RELAY_PIN, LOW); // turn off heating 

}



//////////////////////////////////////////////////////////////////////////
//
// Connect to WiFi
//////////////////////////////////////////////////////////////////////////
void setup_wifi()
{
	// We start by connecting to a WiFi network
#ifdef DEBUG
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(wifiName);
#endif


	WiFi.mode(WIFI_STA);
	WiFi.begin(wifiName, WIFI_PASSWORD);

	while (WiFi.status() != WL_CONNECTED)
	{
    updateOLED(NULL, NULL, "WiFi   ", NULL);
		delay(600);
		Serial.print(".");
		updateOLED(NULL, NULL, "WiFi...", NULL);

	}

	WiFi.hostname(deviceName);
	Serial.println("");
	Serial.print("WiFi connected - ESP IP address: ");
	Serial.println(WiFi.localIP());
	updateOLED(NULL, NULL, "Connected    ", NULL);
}

#define IS_BATTERY_EXTEND_ON  (cost_array[now.array_now].plan & (BATTERY_EXTEND)) !=0
#define IS_BATTERY_SAVE_ON  (cost_array[now.array_now].plan & (BATTERY_SAVE)) !=0
//////////////////////////////////////////////////////////////////////////////////
// call about every 3 seconds
// BATTERY_EXTEND_MODE: stops using the battery under light loads but still 
//                      supplies hevy loads from the battery and allowes charging.
// BATTERYSAVE: stops using the battery but allowes charging.
//////////////////////////////////////////////////////////////////////////////////
void batterySave()
{
	static int8_t	last_run = 0;
  static int grid_power =0;
  static int bat_power =0;
  static uint8_t in_auto =0; //should be same as INVERTER_RUNNINGSTATE
  int m;
  char mode_string[] = "Undefined start up   ";
  char buf[20];
  last_run++;
	if(last_run==BATTERYSAVE_INTERVAL && ((cost_array[now.array_now].plan & BATTERY_SAVE) !=0) || ((cost_array[now.array_now].plan & BATTERY_EXTEND) != 0))
	{
    last_run =0;
    
    m = (int)get_inverter_value(SOFAR_REG_GRIDW);
		if(m == 123456789) // its an error code
      return;
    grid_power = m;// ((grid_power * 1) + m)/2; //recursively avrage to reduse effect of spikes  
    
    m = (int)get_inverter_value(SOFAR_REG_BATTW);
		if(m == 123456789) // its an error code
      return;
    bat_power = m;//((bat_power * 1) + m)/2; //recursively avrage to reduse effect of spikes  

    
    //Serial.println("Monitoring Grid power: "+String(grid_power)+ "W Battery power: "+String(bat_power)+ "W State=" + String(INVERTER_RUNNINGSTATE)+" in_auto="+String(in_auto));// tempary line
  
  
    {
   //  Serial.println("battery save mode on"); 
    // in auto mode    and battery is discharging
    if(INVERTER_RUNNINGSTATE != waiting &&  bat_power < -20   && bat_power > -400    ) // - meens power from battery
      {
        if(IS_BATTERY_EXTEND_ON)
          strcpy(mode_string, "Bat. extend  ");
        else
          strcpy(mode_string, "Battery save ");
      if(!sendPassiveCmd(SOFAR_SLAVE_ID, SOFAR_FN_STANDBY, SOFAR_PARAM_STANDBY, &mode_string[0])) // this makes the battery do nothing
        {
          in_auto = 0;
        //  Serial.println("Grid power: "+String(grid_power)+ "W Battery power: "+String(bat_power)+ "W so switched to standby");
          last_run = -6; // allows time for change before retesting
        }
      }
    // power is flowing to the grid and in standby better go to auto and charge battery	
    if((INVERTER_RUNNINGSTATE == waiting && (grid_power > 20))  || (IS_BATTERY_EXTEND_ON && grid_power < -400    ))
      {
      if(!sendPassiveCmd(SOFAR_SLAVE_ID, SOFAR_FN_AUTO, 0, "bsave_auto   "))  // this can import and export 
        {
        in_auto = 1;
      //  Serial.println("Grid power: "+String(grid_power)+ "W Battery power: "+String(bat_power)+ "W so switched to auto");
        last_run = -6; // allows time for change before retesting checked every 6 seconds normaly 36 seconds after change
        } 
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////
// run every 9 seconds
//
////////////////////////////////////////////////////////////////////////////////////
void heartbeat()
{
	static unsigned long  lastRun = 0;
  lastRun++;
	//Send a heartbeat
	if(lastRun == HEARTBEAT_INTERVAL)
	{
    lastRun =0;
		uint8_t	sendHeartbeat[] = {SOFAR_SLAVE_ID, 0x49, 0x22, 0x01, 0x22, 0x02, 0x00, 0x00};
		int	ret;
#ifdef DEBUG
		Serial.println("Send heartbeat");
#endif
		// This just makes the dot on the first line of the OLED screen flash on and off with
		// the heartbeat and clears any previous RS485 error massage that might still be there.
		if((ret = sendModbus_f(sendHeartbeat, sizeof(sendHeartbeat), NULL)))		
		{
			Serial.print("Bad heartbeat ");
			Serial.println(ret);
			updateOLED(NULL, NULL, "RS485        ", "ERROR        ");
		}

		//Flash the LED
		//digitalWrite(LED_BUILTIN, LOW);
		//delay(4);
		//digitalWrite(LED_BUILTIN, HIGH);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// run every 5sec
//
//////////////////////////////////////////////////////////////////////////////////////////
void updateRunstate()
{
	static unsigned long	lastRun = 0;
  char buf[21];
  char buf_grid[21];
  lastRun++;
	//Check the runstate
	if(lastRun == RUNSTATE_INTERVAL)
	{
    lastRun = 0;
		modbusResponse  response;
#ifdef DEBUG
		Serial.print("Get runstate: ");
#endif
    sprintf(buf,"Bat %dW    ",batteryWatts());
    sprintf(buf_grid,"Grid %5dW  ", (int)get_inverter_value(SOFAR_REG_GRIDW));
		if(!readSingleReg(SOFAR_SLAVE_ID, SOFAR_REG_RUNSTATE, &response))
		{
			INVERTER_RUNNINGSTATE = ((response.data[0] << 8) | response.data[1]);
     // #ifdef DEBUG
			//Serial.println(INVERTER_RUNNINGSTATE);
     // #endif

			switch(INVERTER_RUNNINGSTATE)
			{
				case waiting:
					if(IS_BATTERY_EXTEND_ON)
          {
						updateOLED(NULL, NULL, "Bat. Extend  ", buf_grid);
          }
					else if(IS_BATTERY_SAVE_ON)
          {
						updateOLED(NULL, NULL, "Battery Save ", buf_grid);
          }
          else
          {
						updateOLED(NULL, NULL, "Standby      ", "             ");
          }
					break;

				case check_s:
					updateOLED(NULL, NULL, "Checking     ", NULL);
					break;

				case charging:
					updateOLED(NULL, NULL, HUMAN_CHARGING, &buf[0]);
					break;

#ifdef INVERTER_ME3000
				case checkDischarge:
					updateOLED(NULL, NULL, "Check Dis    ", NULL);
					break;
#endif
				case discharging:
					updateOLED(NULL, NULL, HUMAN_DISCHARGING, &buf[0]);
					break;

				case epsState:
					updateOLED(NULL, NULL, "EPS State    ", NULL);
					break;

				case faultState:
					updateOLED(NULL, NULL, "FAULT        ", NULL);
					break;

				case permanentFaultState:
					updateOLED(NULL, NULL, "PERMFAULT    ", NULL);
					break;

				default:
					updateOLED(NULL, NULL, "Runstate?   ", NULL);
					break;
			}
     // String()
     // updateOLED(NULL, String(get_inverter_value(SOFAR_REG_BATTSOC)),NULL , NULL);
    //  (int)get_inverter_value(SOFAR_REG_BATTSOC)
		}
		else
		{
			Serial.println(response.errorMessage);
			updateOLED(NULL, NULL, "CRC-FAULT    ", NULL);
		}


  
	}
}
// /////////////////////////////////////////////////////////////////////
//
// usees 12345.12345 as an error code
////////////////////////////////////////////////////////////////////////
float get_inverter_value(int code)
{
modbusResponse  rs;
//unsigned
 int16_t	p = 0;
float f;
if(!readSingleReg(SOFAR_SLAVE_ID, code, &rs))
			p = ((rs.data[0] << 8) | rs.data[1]);
		else
    {
      #ifdef DEBUG
			Serial.println("modbus error");	
      #endif
      updateOLED(NULL, NULL, "CRC-FAULT    ", NULL);
      return (123456789);
    }
switch(code)
  {
    case SOFAR_REG_LOADDAY  : f = ((float)p)/100;     break;
    case SOFAR_REG_PVDAY    : f = ((float)p)/100;     break;
    case SOFAR_REG_IMPDAY   : f = ((float)p)/100;     break;
    case SOFAR_REG_GRIDW    :  
    case SOFAR_REG_BATTW    : f= ((float)p)*10;       break; 
                       
    default : f = p;    
  }
 // if(SOFAR_REG_BATTW != 0)
 // {
 // Serial.println("comand "+String(code));  
 // Serial.println("ppp "+String(p));
 // Serial.println(String(String("f watage = ") + String(f,DEC)));
 // }
return(f);
}


////////////////////////////////////////////////////////////////////////////////
//
//
/////////////////////////////////////////////////////////////////////////////////
 int batteryWatts()
{ 
	if(INVERTER_RUNNINGSTATE == charging || INVERTER_RUNNINGSTATE == discharging)
	{  
   return (int)get_inverter_value(SOFAR_REG_BATTW);
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////////////////////////////
void setup()
{
	Serial.begin(9600);
  Serial.println("1 Serial start");  
  Serial2.begin(9600,SERIAL_8N1,RXPin,TXPin); // for RS422
	pinMode(LED_BUILTIN, OUTPUT);  
  pinMode(TFT_LIGHT, OUTPUT);  
  pinMode(RELAY_PIN, OUTPUT);  
  
  digitalWrite(TFT_LIGHT, LOW);
  digitalWrite(RELAY_PIN, LOW);

 // pinMode(27, OUTPUT);  
//  digitalWrite(GREEN_LED, HIGH); // high = 0ff
//  pinMode(RED_LED, OUTPUT); 
 // digitalWrite(RED_LED, HIGH);
//	pinMode(BBLUE_LED, OUTPUT); 
//  digitalWrite(BBLUE_LED, HIGH);


 Serial.print("original CPU speed "); 
  Serial.println(getCpuFrequencyMhz()); 
setCpuFrequencyMhz(80);
Serial.print("new CPU speed "); 
  Serial.println(getCpuFrequencyMhz()); 

  stats.yesterdays_power_use = SEED_POWER_USE; //inital value will learn from previus days use
  stats.yesterdays_outside_min_temp = 15;
  stats.outside_min_temp = 15;
  stats.resulting_soc = 50;

	delay(500);

	//Turn on the screen
	tft.begin();  // initialise screen  
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK); 

 // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(1);

	updateOLED("Sofar CNT", "Connecting   ", NULL, NULL);
//delay(5000);

 digitalWrite(TFT_LIGHT, HIGH);


 
	setup_wifi();
  updateOLED(NULL, "Online      ", NULL, NULL);
  Serial.println("4 uart2 start"); 
  Serial2.begin(9600);
	//Wake up the inverter and put it in auto mode to begin with.
	//heartbeat();

//	Serial.println("Set start up mode: Auto");
 // sendPassiveCmd(SOFAR_SLAVE_ID, SOFAR_FN_AUTO, 0, "Auto        "); //SOFAR_FN_STANDBY

  client.setInsecure();

  clk_timer = timerBegin(2, 80, true);  // set timer to  1MHz
 timerAlarmWrite(clk_timer, 60000000, true); // set alarm to reset after 1 minute
 timerAlarmEnable(clk_timer);  
 updateOLED(NULL, NULL, "HTTP Get time", "Download     ");
 get_web_data(TIME_URL,'t');//get_web_time();  // set time for real time clock
 
 cost_array[0].cost = 125; // use as a flag to say need to get data from web sites
 
  plan_control(); // re run strategy to work out what to do
  implement_control(); // implement strategy when required
 // sendPassiveCmd(SOFAR_SLAVE_ID,SOFAR_FN_STANDBY , 0, "stand_by");  

}

void loop()
{
  int l,soc;
  int8_t up_down=0;
  char buf[30];
  char wifi_buf[30];
  static uint8_t next_adj_time = 0;
  static int wifi_status = -1;
  getTime();

	//Send a heartbeat to keep the inverter awake
	heartbeat();

	//Check and display the runstate
	updateRunstate();

	//Set battery save state
	batterySave();

if(now.min == next_adj_time) // do on the hour and 1/2 hour
{
  if(next_adj_time == 30)
    next_adj_time = 0;
  else
    next_adj_time =30;
  plan_control(); // re run strategy to work out what to do
  implement_control(); // implement strategy when required
}

/* if(wifi_status != WiFi.status()) // state has changed
  {
    wifi_status = WiFi.status();  
    if(WiFi.status() != WL_CONNECTED) 
      strcpy(wifi_buf,"Offline      ");
    else
      strcpy(wifi_buf,"Online*       ");
  }*/
  
  soc = (int)get_inverter_value(SOFAR_REG_BATTSOC) ; 

  sprintf(buf,"SOC %3d     ",soc);
  update_plan_screen(&buf[0],NULL,NULL,NULL,NULL,NULL);
  sprintf(buf,"%02d:%02d:%02d  ",now.hour,now.min,now.sec);
//  Serial.println(buf); 
  updateOLED(buf, wifi_buf, NULL, NULL);

 //digitalWrite(RELAY_PIN, LOW); 
// delay(4000);
 // digitalWrite(RELAY_PIN, HIGH); 


  // check if user tuched the screen
  for(l=0;l<100;l++)
  {
     delay(10); 
    if (ts.touched()) 
      {   
      up_down = do_buttons(); // returns -1 for reduse +1 for increse  and 0 if not on button
      Serial.print("User changing power req = "); 
      Serial.println(up_down); 
      if(up_down !=0)
        {
        stats.forcast_power_rec = stats.yesterdays_power_use = stats.forcast_power_rec + (up_down * (BATTERY_SIZE/MAX_POWER/2));
        plan_control(); // re run strategy to work out what to do as user changed power requirment
        implement_control(); // implement strategy when required   
        }
      else
        draw_grath(20,200);  
    } 
  }
}




