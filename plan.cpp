
/*
Sofar2mqtt/set/standby   - send value "true"
Sofar2mqtt/set/auto   - send value "true" or "battery_save"
Sofar2mqtt/set/charge   - send values in the range 0-3000 (watts)
Sofar2mqtt/set/discharge   - send values in the range 0-3000 (watts)


*/
#include "main.h"


extern WiFiClient wifi;
extern Stats stats;
extern Current_time now;
extern Octopuss cost_array[]; // store all cost data for next day or 2
extern Adafruit_ILI9341 tft;
extern uint8_t LOG_DATA; // flags to say if data should be loged
extern char OCTOPUS_URL[];
extern char WETHER_URL[];
extern int PRICE_LOW_ALLOW_BATTERY_EXTEND;
extern int PRICE_LOW_ALLOW_BATTERY_SAVE;
extern int PRICE_LOW_INCREASE_CHARGING_TIME;
extern int TEMPERATURE_COMPENSATION;
extern float SOLAR_PANEL_COFF;
extern float BATTERY_SIZE;
extern float BATTERY_EFF;
/////////////////////////////////////////////////////////////////////////////////////////////
//
//
/////////////////////////////////////////////////////////////////////////////////////////////
 void plan_control(void) // re run strategy to work out what to do
 {
  char buf[100];
  char buf2[100];
  int x,y;
  uint8_t n=0;
  float cheap=200;
  
  int periods_of_charge=0; // haw many 1/2 hour periods we currently have
  int req_periods_to_charge =0; // how many 1/2 hours are we charging for
  int soc;
  
  
  if((now.hour >  17 &&  now.date + 1 != cost_array[3].date) || cost_array[0].cost == 125)
    get_tomorrows_web_data(); // get octupuse price and forcast data also updates power required.
   
  if(now.hour == 23 && now.min == 30)
    day_update();
    // Stage 1 
  updateOLED(NULL, NULL, "New PLan     ", "             ");
  stats.cheapest = 200;
 // stats.hhours_cost_high =0;
  getTime(); // update so time is definatly past the 00 or 30 minutes
  stats.max_cost = 0; 
  for(x=0;x<70;x++)
  {
    cost_array[x].plan = 0;// Making a new plan so delete the old one
    if(cost_array[x].cost < stats.cheapest)
      {
      stats.cheapest = cost_array[x].cost;
      stats.cheapest_at = x;
      }
    if(cost_array[x].cost > stats.max_cost)
      stats.max_cost = cost_array[x].cost;
    //if(cost_array[x].cost > FLAT_RATE_TARIF) // count how long we will have to supply power
    //  stats.hhours_cost_high++;

    if(cost_array[x].sec_from_midnight < now.sec_from_midnight && cost_array[x].date == now.date) //now
      {
        stats.current_price = cost_array[x].cost;
        break;
      }
  }
  now.array_now = x;
  
  //stage 2 *************************************************************************************
  // calculate how much if any power we nead to import to aviode buying when the price is high 
  req_periods_to_charge = calc_power_required();

  // Find the cheapest power before tomorrow evening
   soc = get_inverter_value(SOFAR_REG_BATTSOC);
   sprintf(buf,"SOC is %d%% recomend charging for %d 1/2 hour periods",soc,req_periods_to_charge);
   Serial.println(buf);
   stats.resulting_soc = soc; // initalise
   stats.total_charging_cost =0;
   periods_of_charge = 0; 
   y=stats.cheapest_at;// start pointing to cheapest
   // get the required charging times or more if very cheap
   Serial.print("About to enter while loop cheepest is ");
   Serial.println(cost_array[y].cost);
  while(periods_of_charge < req_periods_to_charge                                               // we nead it
          || (PRICE_LOW_INCREASE_CHARGING_TIME > cost_array[y].cost && stats.resulting_soc < 60 ) // its cheep
          || ((PRICE_LOW_INCREASE_CHARGING_TIME/2) > cost_array[y].cost && stats.resulting_soc < 80 ) 
          //|| (0.1 > cost_array[y].cost && stats.resulting_soc < 99 ))                            // its free
          || (0.1 > cost_array[y].cost ))                            // its free
  {
    n++;
     Serial.println("while loop ran *****************************");
    if(n > 40)
      break; // escape if conditions are imposible
    cheap = 200;
    for(x=7;x<=now.array_now;x++)
    {   
      if(cost_array[x].cost < cheap && ((cost_array[x].plan & 0xF) & CHARGE) == 0)
        {
        cheap = cost_array[x].cost; 
        y=x; // remember where it is
        }  
    } // end for loop
    if(periods_of_charge != req_periods_to_charge)
       cost_array[y].plan = CHARGE ; // needed use this one 
    if(cost_array[y].cost < cheap)
       cost_array[y].plan = CHARGE ; //cheap or needed use this one     
    if(0.1 > cost_array[y].cost)
       cost_array[y].plan = CHARGE + HOT_WATER + HEAT; // free use this one 
    

#ifdef DEBUG
     dtostrf(cost_array[y].cost,4,2, buf2);
     sprintf(buf," %d Paying %sp at %02d:%02d  SOC becomes %d",buf2,x,cost_array[y].hour,cost_array[y].min,stats.resulting_soc);
     Serial.println(buf);
#endif
     stats.total_charging_cost = stats.total_charging_cost + (cost_array[y].cost * (((float)MAX_POWER)/2000.0));
    // periods_of_charge++;
     stats.resulting_soc = soc + ( (BATTERY_SIZE/(MAX_POWER/2.0)) * periods_of_charge * (BATTERY_EFF/100.0));
    // stats.resulting_soc = soc + (7.93 * periods_of_charge);
     sprintf(buf,"periods charg = %d  new soc =%d  plan = %x",periods_of_charge,stats.resulting_soc,cost_array[y].plan);
     Serial.println(buf);
    periods_of_charge++;
  } // end while
  
    char buf3[6];    
    dtostrf(((float)periods_of_charge/2),3,1, buf3); // Charging for 1h to get to 30% soc, costing 30.4p
    sprintf(buf,"Charging for %sh to get",buf3);
    dtostrf(stats.total_charging_cost,4,2, buf3);   
    sprintf(buf2,"to %d%% SOC, costing %sp ",stats.resulting_soc, buf3);
    update_plan_screen(NULL,NULL,NULL,&buf[0], &buf2[0],NULL);

  // Stage 3 
  // work out when we should buy a little instead of using the battery
  // this is when the load is low and the cost resanabul also depends on soc
  int price_low_allow_battery_extend  = PRICE_LOW_ALLOW_BATTERY_EXTEND + (PRICE_LOW_ALLOW_BATTERY_EXTEND * ((float)(100.0-soc))/80);
  int price_low_allow_battery_save  = PRICE_LOW_ALLOW_BATTERY_SAVE + (PRICE_LOW_ALLOW_BATTERY_SAVE * ((float)(100.0-soc))/80);
#ifdef DEBUG
  dtostrf(price_low_allow_battery_extend,4,2, buf2);
  sprintf(buf,"Battery extend at cost below %s",buf2);
  Serial.println(buf);
  dtostrf(price_low_allow_battery_save,4,2, buf2);
  sprintf(buf,"Battery save at cost below %s",buf2);
  Serial.println(buf);
#endif
 getTime();
 for(x=9;x<now.array_now+1;x++)
    { 
      if(stats.cheapest > 0 || stats.cheapest_at > x)  // this stopes us buying power just before it is free
      {
        if(cost_array[x].cost < price_low_allow_battery_extend && ((cost_array[x].plan & 0xF) & CHARGE) == 0) // if cheap and not charging  
        {  
          if(cost_array[x].cost < price_low_allow_battery_save)      
            cost_array[x].plan = cost_array[x].plan | BATTERY_SAVE;  // if very cheep dont use battery   
          else
            cost_array[x].plan = cost_array[x].plan | BATTERY_EXTEND; // if mid priced save constant drain on battery but supply cups of tea
          // Serial.println("add to battery save " + String(x) +"    "+ String(cost_array[x].plan));
        }   
      }   
    }



// display the plan
sprintf(buf,"Date %dth current time %02d:02%d",cost_array[now.array_now].date,cost_array[now.array_now].hour,cost_array[now.array_now].min);
Serial.println(buf);

Serial.println("Cost  battery extend  battery save  charge");
  for(x=now.array_now; x>-1;x--)
    { 
      sprintf(buf,"%02d:%02d,   %f, %x ",cost_array[x].hour,cost_array[x].min,cost_array[x].cost,cost_array[x].plan);    
      Serial.println(buf);  
    }
    draw_grath(20,200);
     



    //sprintf(buf,"Cost %sp for %sh   ",buf2,buf3);  
   
 //sprintf(buf2,"Charging to %d%% SOC ",stats.resulting_soc);



    
    getTime(); // refresh as log file can take a long time
     // an optinal log file adding a line every 30 minutes
    if((LOG_DATA&1) == 1)
        send_ss_hh_data( cost_array[now.array_now].plan,  soc,  ((float)periods_of_charge/2), stats.total_charging_cost);
    if((LOG_DATA&8) == 8)
      log_SD_hh_data( cost_array[now.array_now].plan,  soc,  ((float)periods_of_charge/2), stats.total_charging_cost);    
    getTime();
 }





// /////////////////////////////////////////////////////////////////////
//
// calculate how much if any power we need to import to aviode buying when the price is high
// forcast_power_rec = global value generated based on user input or history
// 
// ///////////////////////////////////////////////////////////////////// 
int calc_power_required(void) // assumes no sun
{
  int req_power;
  float req_time;
  int soc;
  int available_power;
  int forcast_power_recquired;
  int8_t deltaT;
  char buf[100];
  char buf2[12];

 forcast_power_recquired =  stats.forcast_power_rec; // this value uptates once a day only?????????????????
// find out how much power we have now
  soc = (int)get_inverter_value(SOFAR_REG_BATTSOC) ; 
  available_power = ((BATTERY_SIZE)/100)*(soc-20); // in in  watts subtract last 20% as it can not be used

#ifdef SERIAL_INFO
  Serial.println("************** Control Plan is ****************");
  Serial.println("Cheapest "+ String(stats.cheapest));
  Serial.println("current_price "+String(stats.current_price));
  Serial.println("1/2 hour periods at high cost "+String(stats.hhours_cost_high));
  Serial.println("Min tempratuer tomorrow "+String(stats.tomorow_outside_min_temp));
  Serial.println("Max tempratuer tomorrow "+String(stats.tomorow_outside_max_temp));
  Serial.println("Solar energy tomorrow "+String(stats.tomorow_solar_energy));
  Serial.println("today_purchase "+String(get_inverter_value(SOFAR_REG_IMPDAY)));
  Serial.println("today_consumption "+String(get_inverter_value(SOFAR_REG_LOADDAY)));
  Serial.println("today_generation "+String(get_inverter_value(SOFAR_REG_PVDAY))); 
  sprintf(buf,"Availabul power %dW SOC %d",available_power,soc);
  Serial.println(buf);
#endif
  // Subtract expected power from sun ***********************************
 forcast_power_recquired = forcast_power_recquired - stats.estemated_solar_power;
  // adjust for tempretuer if its colder than yesterday we will probably want more power and if warmer less power
  deltaT = stats.outside_min_temp - stats.yesterdays_outside_min_temp;
  forcast_power_recquired = forcast_power_recquired + (deltaT * TEMPERATURE_COMPENSATION);

// adjust for very cheap power is done in plan_control() not here
 
  req_power = forcast_power_recquired - available_power; // in watts
  req_power = req_power * (100.0/BATTERY_EFF); // correct for losses
  if(req_power < 0) 
    req_power = 0;
#ifdef SERIAL_INFO
  sprintf(buf,"Forcast_power_recquired %d",req_power);
  Serial.println(buf);
#endif


   
   if(req_power > BATTERY_SIZE-available_power) // want more than will fit in battery
   {
    if(stats.cheapest < PRICE_LOW_INCREASE_CHARGING_TIME)
      req_power = BATTERY_SIZE - available_power ; // its cheap so will charge to 100%
    else // hope for sunsine or use less!
       req_power = (BATTERY_SIZE - available_power)*0.8; // charge to 80%
   }

     dtostrf(((float)stats.estemated_solar_power)/1000,3,1, buf2);
    sprintf(buf,"SolarForcast %sKWh %d%%",buf2,(int)((((float)stats.estemated_solar_power * (BATTERY_EFF/100.0))/BATTERY_SIZE)*100));
    update_plan_screen(NULL,buf ,NULL,NULL,NULL,NULL);
    
    sprintf(buf,"Power required %dW  ",req_power);
    update_plan_screen(NULL,NULL,buf ,NULL,NULL,NULL);
    if(req_power <= 0)
      return 0; // we have anothe stored no nead to charge
   req_time = (req_power*2/MAX_POWER); // in 1/2 hours

 return (int)(req_time);
   
 //  return (int)(req_time-0.1)+1; // generaly round up unless its .1 or less  
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void get_tomorrows_web_data(void)
{
    if(WiFi.status() != WL_CONNECTED)   // check we still have a wifi connection. reconect if not.
    {
      sendPassiveCmd(SOFAR_SLAVE_ID, SOFAR_FN_AUTO, 0, "Auto         "); //put in a safe condition incase wifi can not reconnect
      setup_wifi(); 
    }                      
    //Serial.print("Octopus ");
    updateOLED(NULL, NULL, "HTTP Octopus ", "Downloading? ");
    getTime();
    get_web_data(OCTOPUS_URL,'o');//get_cost_data(); // call once a day about 5:00PM then try every 1/2 hour untill sucsesful
    //Serial.print("Forcast      ");
    updateOLED(NULL, NULL, "HTTP Forcast ", NULL);
    getTime();
    get_web_data(WETHER_URL,'w'); 
    stats.forcast_power_rec = stats.yesterdays_power_use; // update once a day now so there is time to see the plan in the evening
    stats.estemated_solar_power = stats.tomorow_solar_energy * SOLAR_PANEL_COFF; // prodict howe much power the sun will give tomorrow
  }

////////////////////////////////////////////////////////////////////////////
// Once a day update some stats
// Have to do this at end of day to get total days power. But dont want the plan to change masively when i am asleep
// therefore will use a day out of date data for plan
//
//
////////////////////////////////////////////////////////////////////////////
void day_update(void)
{
  stats.yesterdays_power_use = ((stats.yesterdays_power_use * 3) + get_inverter_value(SOFAR_REG_LOADDAY))/4; // recursively avrage 


// log file
 
  if((LOG_DATA&2) == 2)
  {
    getTime();
    send_ss_day_data(stats.today_solar_energy * SOLAR_PANEL_COFF, get_inverter_value(SOFAR_REG_PVDAY), get_inverter_value(SOFAR_REG_BATTSOC), get_inverter_value(SOFAR_REG_LOADDAY),stats.forcast_power_rec ); // log data 
  }
 if((LOG_DATA&4) == 4)
    log_SD_day_data(stats.today_solar_energy * SOLAR_PANEL_COFF, get_inverter_value(SOFAR_REG_PVDAY), get_inverter_value(SOFAR_REG_BATTSOC), get_inverter_value(SOFAR_REG_LOADDAY),stats.forcast_power_rec ); // log data 
      
 getTime();
}

