
#ifndef MAIN
#define MAIN 1

//#define DEBUG 1

#define WIFI_SSID "Voyager 30" //	"TALKTALK96DCBC"//"realme 6"//"Voyager 30"
#define WIFI_PASSWORD "if6tdt5q" //"PC7HX67T"	//"elf2bonchance"//"if6tdt5q"



// defines for now should be changed based on forcast
#define FLAT_RATE_TARIF 35 // never want to pay more than this per unit
// folowing 2 values are modified as SOC falles up to x2 at 20% soc  
#define PRICE_LOW_ALLOW_BATTERY_EXTEND 15 //   *2 at 20% soc if cost is less than this power will come from grid not battery when load is small
#define PRICE_LOW_ALLOW_BATTERY_SAVE 10 // when the price is below this the mains will be used to save battery charge
#define PRICE_LOW_INCRESE_CHARGING_TIME 9 // if price is less than this then allow more power to be stored than reqiored for today
#define BATTERY_SIZE 11900.0 // the capasity of the battery in W
#define BATTERY_EFF 84.0 // efficience of inverter ie 1KW in 0.84KW
#define SEED_POWER_USE 3000 // W's required to add to battery/day. 
                            // This is a starting point the actuel value will be based on history once the cpu has been running for a few days
#define TEMPRATUER_COMPENSATION 220 // If you have electrical heating The power use is havely dependent on outside tempretuer. This value modifies
                                    // the reqired_power = required_powet + (deltaT * TEMPRATUER_COMPENSATION). Set to zero to disable
#define SOLAR_PANEL_COFF 0.4 // This is multiplied by the forcast solor radiation for 1m squerd. To get this value use number_of_m_sq_solar_panls/ panel efficiency ()
                              // eg 4m sq of soler panels with 12% efficiensy would be 4/12 = 0.33. To play safe when the forcast isent good you can make this number smaller
                              // if too big you may run out power at peak time. If to low you may reach 100% charge and export to grid having bought too much power the night before.

#define TIME_URL "https://worldtimeapi.org/api/ip"
// change for your reagen and tarref
#define OCTOPUSS_URL "https://api.octopus.energy/v1/products/AGILE-FLEX-22-11-25/electricity-tariffs/E-1R-AGILE-FLEX-22-11-25-J/standard-unit-rates/"
#define WETHER_URL "https://api.open-meteo.com/v1/forecast?latitude=51.003293&longitude=0.396079&daily=apparent_temperature_max,apparent_temperature_min,sunshine_duration&forecast_days=3"
#define DATA_TO_SS "https://script.google.com/macros/s/AKfycbyufeQCFAvoOzhuiEZw0fGEf0NNo_0-t7f5ZgHkvI1ymZ5vtVDKWg7v3h3SFcRoJMkGhQ/exec?temperature=20.00&humidity=1.00"

// defines used for forcast and implementation
#define CHARGE  1
#define BATTERY_SAVE (1<<1) // flag to say save whats in the battery and use mains power
#define BATTERY_EXTEND (1<<2) // compromise use the battery for hevy loads but not light loads to extend battery life.

#define HEAT  (1<<4)  // flag to say its a good time to put the heating on
#define HOT_WATER (1<<5)



 // Update these to match your inverter/network.
#define INVERTER_ME3000				// Uncomment for ME3000
//#define INVERTER_HYBRID			// Uncomment for Hybrid
//#define DEBUG
#ifdef INVERTER_ME3000
#define	MAX_POWER		3000.0		// ME3000 is 3000W max.
#elif defined INVERTER_HYBRID
#define MAX_POWER		6000.0
#endif

#define RS485_TRIES 8       // x 50mS to wait for RS485 input chars.

#define SOFAR_SLAVE_ID          0x01
 // SoFar ME3000 Information Registers
#define SOFAR_REG_RUNSTATE	0x0200
    #define SOFAR_FN_STANDBY	0x0100
    #define SOFAR_FN_DISCHARGE	0x0101
    #define SOFAR_FN_CHARGE		0x0102
    #define SOFAR_FN_AUTO		0x0103
#define SOFAR_REG_GRIDV		0x0206
#define SOFAR_REG_GRIDA		0x0207
#define SOFAR_REG_GRIDFREQ	0x020c
#define SOFAR_REG_BATTW		0x020d
#define SOFAR_REG_BATTV		0x020e
#define SOFAR_REG_BATTA		0x020f
#define SOFAR_REG_BATTSOC	0x0210
#define SOFAR_REG_BATTTEMP	0x0211
#define SOFAR_REG_GRIDW		0x0212
#define SOFAR_REG_LOADW		0x0213
#define SOFAR_REG_SYSIOW	0x0214
#define SOFAR_REG_PVW		0x0215
#define SOFAR_REG_PVDAY		0x0218
#define SOFAR_REG_EXPDAY	0x0219
#define SOFAR_REG_IMPDAY	0x021a
#define SOFAR_REG_LOADDAY	0x021b
#define SOFAR_REG_BATTCYC	0x022c
#define SOFAR_REG_PVA		0x0236
#define SOFAR_REG_INTTEMP	0x0238
#define SOFAR_REG_HSTEMP	0x0239
#define SOFAR_REG_PV1		0x0252
#define SOFAR_REG_PV2		0x0255

// Sofar run states
#define waiting 0
#define check_s 1

#ifdef INVERTER_ME3000
#define charging		2
#define checkDischarge		3
#define discharging		4
#define epsState		5
#define faultState		6
#define permanentFaultState	7

#define HUMAN_CHARGING		"Charging    "
#define HUMAN_DISCHARGING	"Discharge   "
#elif defined INVERTER_HYBRID
#define normal			2
#define epsState		3
#define faultState		4
#define permanentFaultState	5
#define normal1			6

// State names are a bit strange - makes sense to also match to these?
#define charging		2
#define discharging		6

#define HUMAN_CHARGING		"Normal"
#define HUMAN_DISCHARGING	"Normal1"
#endif

#define DS3231_I2C_ADDRESS          0x68   ///< I2C ADDRESS

#define DS3231_SEC          0x00   ///< RTC Seconds Register
#define DS3231_MIN          0x01   ///< RTC Minutes Register
#define DS3231_HOUR         0x02   ///< RTC Hours Register
#define DS3231_DAY          0x03   ///< RTC Day Register
#define DS3231_DATE         0x04   ///< RTC Date Register
#define DS3231_MONTH        0x05   ///< RTC Month/Century Register
#define DS3231_YEAR         0x06   ///< RTC Year Register
#define DS323X_REG_CONTROL          0x0E   ///< Control Register
#define DS323X_REG_STATUS           0x0F   ///< Status Register
#define DS323X_REG_AGE_OFFSET       0x10   ///< Aging Offset Register
#define DS3231_TEMPERATURE      0x11   ///< temperature Register

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <HTTPClient.h>

// SoftwareSerial is used to create a second serial port, which will be deidcated to RS485.
// The built-in serial port remains available for flashing and debugging.
//#include <SoftwareSerial.h>
//#define SERIAL_COMMUNICATION_CONTROL_PIN -1// D5 // Transmission set pin
#define RS485_TX HIGH
#define RS485_RX LOW 
#define RXPin 22//27  // Serial Receive pin
#define TXPin 27 //22  // Serial Transmit pin

// screen pins (require setting to truw values)
#define TFT_CS 15 //ok
#define TFT_DC 2 //   2ok 18
#define TFT_MOSI 13 //ok i019
#define TFT_CLK 14 //ok
#define TFT_MISO -1 // suspect 23
#define TFT_RESET 12
#define TFT_LIGHT 21

#define RED_LED GPIO17 //io17  4 is wrong
#define RELAY_PIN 17 //was blue led
//#define BBLUE_LED 17
//4 pin connector 1=io21 2=io22 3 =1o35 4=GND // 35 input only

// ----------------------------
// Touch Screen pins
// ----------------------------
// The CYD touch uses some non defaultspi pins
#define VSPI  3 //SPI bus normally attached to pins 5, 18, 19 and 23, but can be matrixed to any pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33


#define MAX_FRAME_SIZE          64
#define MODBUS_FN_READSINGLEREG 0x03
#define SOFAR_FN_PASSIVEMODE    0x42
#define SOFAR_PARAM_STANDBY     0x5555


// This is the return object for the sendModbus() function. Since we are a modbus master, we
// are primarily interested in the responses to our commands.
//#ifndef modbusResponse
struct modbusResponse
{
	uint8_t errorLevel;
	uint8_t data[MAX_FRAME_SIZE];
	uint8_t dataSize;
	char errorMessage[50];
};
//#endif 

struct Current_time{
  uint8_t day;
  uint8_t date;
  uint8_t hour;
  uint8_t min;
  int sec;
  uint8_t array_now; // the location of the octopuss price data covering the current time
  long sec_from_midnight; // a singel value to make comparisons easier
}; 
// cost of electricity for 1/2 an hour slot
struct Octopuss
{
  uint8_t date;
  uint8_t hour;
  uint8_t min;
  uint8_t day;
  long sec_from_midnight;
  float cost;
  uint8_t plan; // flage bits for what is planned for this 1/2 hour
  }; 

struct Stats
{
int yesterdays_power_use; // the power taken from the battery previusly used for estemaiting to days requirment
int8_t yesterdays_outside_min_temp;
int today_solar_energy;

uint8_t hhours_cost_high; // number of periods the cost is above set limit 35p??
int8_t outside_min_temp; // from yesterdays forcast
float cheapest; // lowest cost found in data
uint8_t cheapest_at; // the x location in the array wher cheapest is to be found
float max_cost; // most expensive
float total_charging_cost; // total cost for forcast charging
int resulting_soc; // the state of charge expected after charging
float current_price; 

int forcast_power_rec = 1400; // power/half hour to be modified by actuel use

int8_t tomorow_outside_min_temp;
int8_t tomorow_outside_max_temp;
int tomorow_solar_energy; // w/m2
int estemated_solar_power; // how many W we expect to generate tomorrow


};



void getTime(void);
void tdelay(int del);
int batteryWatts(void);
void setup_wifi(void);
bool checkCRC(uint8_t frame[], byte frameSize); 
void calcCRC(uint8_t frame[], byte frameSize); 
int addStateInfo(String &state, uint16_t reg, String human);
int listen(modbusResponse *resp);
int readSingleReg(uint8_t id, uint16_t reg, modbusResponse *rs);

void plan_control(void);
int calc_power_required(void);
int get_web_data(String apiURL,char name);

float get_inverter_value(int code);
void flushRS485(void);
int sendModbus_f(uint8_t frame[], byte frameSize, modbusResponse *resp);
//int readSingleReg(uint8_t id, uint16_t reg, modbusResponse *rs);
int sendPassiveCmd(uint8_t id, uint16_t cmd, uint16_t param, const char* pubTopic);
String getValue(HTTPClient &http, String key, int skip, int get);
void do_octopuss_web(HTTPClient &http);
void  do_time_web(HTTPClient &http);
void  do_wether_web(HTTPClient &http);
void updateOLED(const char *line1, const char* line2, const char *line3, const char *line4);
void draw_grath(int x, int y);
void update_plan_screen(char *line1, char *line2, char *line3, char *line4, char *line5, char *line6);
void get_tomorrows_web_data(void);
void day_update(void);
void send_ss_hh_data(int plan, int soc, float periods_of_charge, float total_charging_cost);
void send_ss_day_data(int today_solar_energy, int pv_day, int soc, int load_day, int forcast_power_rec ); // log data 
int8_t do_buttons(void);
#endif