/////////////////////////////////////////////////////////////////////
//This code is used to control a switch from the Blynk app via wifi.
//The relay is connected to output GPIO2.
//Virtual pin V0 has a direct control on GPIO2 of this switch
//Virtual pin V1 is used as a feedback to the mobile to indicate if GPIO2 is on
//Virtual pin V2 has a direct control on GPIO2 of a group of several switch
//Virtual pin V3 contains a start and stop time, and a selection of weekdays, to turn on/off the switch like a timer
//Virtual pin V4 is used to activate/deactivate the timer function of virtual pin V3
//
//if V0 or V2 is == 1, then output 2 is HIGH
//else if V4 is == 1 AND time is in the V3 range, then output 2 is HIGH
//else, then output 2 is LOW
///////////////////////////////////////////////////////////////////////

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WidgetRTC.h>
#include <SimpleTimer.h>

// WiFi credentials:
char auth[] = "472ea7b9976d46b396a65dc96d15eae4";
char* ssid[] = {"SSID1", "SSID2"};//replace SSID1 and SSID2 by your SSID names. If a 3rd SSID name has to be added, nbKnownAP = 3
char* pass[] = {"PASS1", "PASS2"};//replace PASS1 and PASS2 by your passwords. If a 3rd password has to be added, nbKnownAP = 3
int nbKnownAP = 2;//number of elements in ssid[] and pass[]
char ssidChosen[20] = "\0";//increase the number of characters if longest ssid is longer than 20 characters
char passChosen[20] = "\0";//increase the number of characters if longest pass is longer than 20 characters

IPAddress ip (45,55,96,146);//replace with your local server

//Global variables:
int currentSecOfDay = 0;//seconds passed from midnight to now
int timerStartSecOfDay = 0;//seconds passed from midnight to the start of the timer
int timerStopSecOfDay = 0;//seconds passed from midnight to the stop of the timer
bool timerWeekDay[7];//array of bool: i is true if timer is set for day i, sunday: i=0
bool timerSet = false;//true if the timer has been set
bool V0ON = false;//true when the switch is turned on via virtual pin V0
bool V2ON = false;//true when the switch is turned on via virtual pin V2
bool lastV1 = false; //Last state of led1
WidgetLED led1(1); //virtual pin 1 controls led1, which is a feedback to the app that tells if GPIO2 is on or off
WidgetRTC rtc;//Real Time Clock widget
SimpleTimer timer;


BLYNK_WRITE(V0)
{
  if(param.asInt() == 1)
    V0ON = true;
  else
    V0ON = false;
  checkStatus();
}

BLYNK_WRITE(V2)
{
  if(param.asInt() == 1)
  {
    Blynk.virtualWrite(0, HIGH);//when switch is turned on via V2, the V0 button also turns on
    V0ON = true;
    V2ON = true;
  }
  else
  {
    Blynk.virtualWrite(0, LOW);//when switch is turned off via V2, the V0 button also turns off
    V0ON = false;
    V2ON = false;
  }
  checkStatus();
}

BLYNK_WRITE(V3)
{
  int i;
  TimeInputParam t(param);

  //Note:
  //In Blynk app, monday -> sunday = 1 -> 7
  //In time library, sunday -> saturday = 1 -> 7
  //In timerWeekDay tab, sunday -> saturday = 0 -> 6
  
  for (i = 1; i <= 7; i++)//to deactivate the timer in the app, uncheck every weekday
  {
    if (t.isWeekdaySelected(i))
      timerWeekDay[i%7] = true;
    else
      timerWeekDay[i%7] = false;
  }
  //calculation of the number of seconds from midnight for the start time and stop time:
  timerStartSecOfDay = t.getStartSecond() + 60*t.getStartMinute() + 3600*t.getStartHour();
  timerStopSecOfDay = t.getStopSecond() + 60*t.getStopMinute() + 3600*t.getStopHour();
}

BLYNK_WRITE(V4)
{
  if(param.asInt() == 1)
    timerSet = true;
  else
    timerSet = false;
  checkStatus();
}

//The function checkStatus() checks if GPIO2 has to be turned on or off.
//It is called every time V0, V2 or V3 values are changed, and every 10 seconds
void checkStatus()
{
  if(V0ON || V2ON)
  {
    if(!lastV1)//if the output has not been turned on yet
    {
      digitalWrite(2, HIGH);
      led1.on();
      lastV1 = true;//to prevent turning it on when it's already on, or off when it's already off
      Serial.println("ON");//for debug purposes
    }
  }
  else if(timerSet)//in case the timer has been set
  {
    if(timerWeekDay[weekday()-1])//if the timer has been set for today
    {
      currentSecOfDay = second()+60*minute()+3600*hour();//calcuates the number of seconds from midnight to now
      if(currentSecOfDay >= timerStartSecOfDay && currentSecOfDay < timerStopSecOfDay)//if current time is in the "ON" range
      {
        if(!lastV1)//if the output has not been turned on yet
        {
          digitalWrite(2, HIGH);
          led1.on();
          lastV1 = true;//to prevent turning it on when it's already on, or off when it's already off
          Serial.println("ON");//for debug purposes
        }
      }
      else//if current time is NOT in the "ON" range
      {
        if(lastV1)//if the output has not been turned off yet
        {
          digitalWrite(2, LOW);
          led1.off();
          lastV1 = false;//to prevent turning it on when it's already on, or off when it's already off
          Serial.println("OFF");//for debug purposes
        }
      }
    }
  }
  else
  {
    if(lastV1)//if the output has not been turned off yet
    {
      digitalWrite(2, LOW);
      led1.off();
      lastV1 = false;//to prevent turning it on when it's already on, or off when it's already off
      Serial.println("OFF");//for debug purposes
    }
  }
}

void setup()
{
  int numSsid = 0;//Number of ssid found
  int i = 0;
  int j = 0;

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  
  Serial.begin(9600);

  Serial.println("** Scan Networks **");
  numSsid = WiFi.scanNetworks();
  if (numSsid == -1)//In case there is no wifi connection
  {
    Serial.println("Couldn't get a wifi connection");
    while (true)
    {delay(1000);}
  }

  // print the list of networks seen:
  Serial.print("number of available networks:");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (i = 0; i < numSsid; i++)
  {
    Serial.print(i);
    Serial.print(") ");
    Serial.print(WiFi.SSID(i));
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm");
    for(j=0; j<nbKnownAP; j++)
    {
      if(WiFi.SSID(i) == ssid[j])//in case 1 of the networks found is a known network
      {
        Serial.print(ssid[j]);
        Serial.println(" is a known network!");
        strcpy(ssidChosen, ssid[j]);
        strcpy(passChosen, pass[j]);
      }
    }
  }
  
  if(strlen(ssidChosen) == 0)//in case no known network was found
  {
    Serial.println("None of the known networks has been found");
    while (true)
    {delay(1000);}
  }

  Blynk.begin(auth, ssidChosen, passChosen, ip, 8442);
  //Blynk.begin(auth, ssid, pass);//Use this line instead of the above if no local server

  while(Blynk.connect() == false);

  rtc.begin();//Begin synchronizing time

  timer.setInterval(10000L, checkStatus);

  Blynk.syncAll();//Syncs all values from the app
}

void loop()
{
  Blynk.run();
  timer.run();
}

