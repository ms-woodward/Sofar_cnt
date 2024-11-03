// screen drawing code 
#include "main.h"


extern Adafruit_ILI9341 tft;
extern XPT2046_Touchscreen ts;
extern Stats stats;
extern Current_time now;
extern Octopuss cost_array[71]; // store all cost data for next day or 2
////////////////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////////////////
void update_plan_screen(char *line1, char* line2, char *line3, char *line4, char *line5, char *line6)
{
	int16_t x=160;
  char buf[15];
  buf[13] = NULL;
  tft.setCursor(x, 1);
  tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);  
  tft.setTextSize(2);	

	if(line1 != NULL)
	{
    strncpy(buf,line1,12); // clip length if too long to fit
		tft.println(buf);
	}
  tft.setTextSize(1);	
	tft.setCursor(x,20);

	if(line2 != NULL)
		tft.println(line2);	

	tft.setCursor(x,30);
	if(line3 != NULL)
		tft.println(line3);	
		
	tft.setCursor(x,40);
	if(line4 != NULL)
		tft.println(line4);	

  tft.setCursor(x,50);
	if(line5 != NULL)
		tft.println(line5);	
  tft.setCursor(x,60);  
  if(line6 != NULL)
		tft.println(line6);	  
}


////////////////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////////////////
void updateOLED(const char *line1, const char* line2, const char *line3, const char *line4)
{
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);  
  tft.setTextSize(1);	

	if(line1 != NULL)
	{
		tft.println(line1);
	}

 tft.setTextSize(2);	
	tft.setCursor(0,11);
	if(line2 != NULL)
	{
    if(strlen(line2) > 13)
        Serial.println("2 too long");
		tft.println(line2);	
	}	

	tft.setCursor(0,35);
	if(line3 != NULL)
  {
    if(strlen(line3) > 13)
        Serial.println("3 too long");
	
		tft.println(line3);	
	}
	
	tft.setCursor(0,55);
	if(line4 != NULL)
  {
    if(strlen(line4) > 13)
        Serial.println("4 too long ");
	
		tft.println(line4);	
	}	
}





#define scaily -2.6
// ////////////////////////////////////////////////////////////////////////////
// draw a bar chart of cast data starting at x and y
//
// /////////////////////////////////////////////////////////////////////////////
void draw_grath(int x, int y)
{
  int8_t s;
  int colour;
  //char buf[8];
  
  tft.fillRect(0, y-150, ILI9341_TFTHEIGHT, y, ILI9341_BLACK); // clear aria
  for(s=0;s<71; s++)
  {
    switch(cost_array[s].plan & 0xF)
    {
      case CHARGE : colour = ILI9341_RED;                       break;
      //case CHARGE+HEAT+HOT_WATER : colour = ILI9341_RED;        break;
      case BATTERY_SAVE : colour = ILI9341_YELLOW;              break;
      case BATTERY_EXTEND : colour = ILI9341_ORANGE;            break;
      default : colour = ILI9341_WHITE;                         break;
    }
    
    tft.drawFastVLine(x+((70-s)*4), y, cost_array[s].cost*scaily+1, colour);
    tft.drawFastVLine(x+1+((70-s)*4), y, cost_array[s].cost*scaily, colour);
    tft.drawFastVLine(x+2+((70-s)*4), y, cost_array[s].cost*scaily-1, colour);
  }
   tft.drawFastVLine(x+((70-now.array_now)*4), y-100, 110, ILI9341_PINK); // mark current time
   tft.setTextSize(1);
   tft.setTextColor(ILI9341_PINK,ILI9341_BLACK); 
   tft.setCursor(x+((70-now.array_now)*4)-10,y+10);
   tft.println("now");
   tft.drawFastHLine(0,(y+cost_array[now.array_now].cost*scaily), 260, ILI9341_PINK); // mark current price
   tft.setCursor(x-20,y-(-cost_array[now.array_now].cost*scaily)-3);
   
  //sprintf(buf,"%fp",cost_array[now.array_now].cost);
  tft.print(cost_array[now.array_now].cost);

  tft.setCursor(x-20,y-(-stats.cheapest*scaily)+12);
  tft.print(stats.cheapest);
 tft.setCursor(x-20,y-(-stats.max_cost*scaily)-5);
  tft.print(stats.max_cost);

  tft.setCursor(x+30,y+20);
  tft.setTextColor(ILI9341_RED,ILI9341_BLACK);
  tft.print("Charge  ");
   tft.setTextColor(ILI9341_ORANGE,ILI9341_BLACK);
  tft.print("Battery extend  ");
  tft.setTextColor(ILI9341_YELLOW,ILI9341_BLACK);
  tft.println("Battery save");
}




int8_t do_buttons(void)
{
  int l;
   int x,y;
  int8_t up_down =0;
  Adafruit_GFX_Button button_d;
  Adafruit_GFX_Button button_i;
  TS_Point tuched;
  tft.setCursor(15, 80);
  tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);  
  tft.setTextSize(2);	
  tft.print("Change Power to be stored");
  button_d.initButtonUL(&tft, 30, 100, 100,45 , ILI9341_GREEN, ILI9341_LIGHTGREY,ILI9341_BLACK, "Reduse", 2);
  button_d.drawButton(true); 
  button_i.initButtonUL(&tft, 200, 100, 100,45 , ILI9341_GREEN, ILI9341_LIGHTGREY,ILI9341_BLACK, "Increse", 2);
  button_i.drawButton(true); 

while(!ts.bufferEmpty())
  {
    tuched = ts.getPoint();
  } // flush the buffer
 
 Serial.println("touch buffer is empty looking for button press now ");

for(l=0;l<27000;l++) // a time out if nothing is pressed
{
    if (!ts.bufferEmpty()) 
      tuched = ts.getPoint(); 
   
    if(tuched.z > 10) // if finger is pressing
      {      
      x=(tuched.x-167)*(320.0/3926); // calobrate
      y=(tuched.y-276)*(240.0/3839);
      }
    else
      {
        x=y=0;
      }
    Serial.print("Pressure = ");
    Serial.print(tuched.z);
    Serial.print(", x = ");
    Serial.print(x);
    Serial.print(", y = ");
    Serial.print(y);
    Serial.println();

  if (button_d.contains(x,y) && tuched.z > 100 )
    button_d.press(true);  // tell the button it is pressed
  else 
    button_d.press(false); 
  
  if(button_d.justPressed())
    button_d.drawButton(false);

  if(button_d.justReleased())
    {   
    up_down = -1;
    Serial.print("*******************decrese button detected*********** ");
    break;
    }

  if (button_i.contains(x,y) && tuched.z > 100)
    button_i.press(true);  // tell the button it is pressed
  else 
    button_i.press(false); 

  if(button_i.justPressed())
    button_i.drawButton(false);

  if(button_i.justReleased())
    {
      up_down = 1;
      Serial.print("increse button detected*** ");
      break;// add stuff here ti reduse power
    }  
  delay(50);  
}
Serial.print("Selection done =  ");
Serial.println(up_down); 
 return up_down;
}

