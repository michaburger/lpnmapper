/*
    Tuino GPS mapper state machine
    
    Created on: March 9, 2018
    
        Author: Micha Burger michaburger92@gmail.com
        Brief: LoRaWAN GPS mapper for signal strength
        Version: 1.0

        Based on: Tuino LoRa example by Gimasi

        License: it's free - do whatever you want! ( provided you leave the credits)

*/

#include "gmx_lr.h"
#include "Regexp.h"
#include "SeeedOLED.h"
#include "display_utils.h"
#include <TinyGPS++.h>

#include <Wire.h>

//DHT
#include "DHT.h"
#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//State machine definitions
#define NO_GPS          0
#define NO_FIX          1
#define CHECK_PRECISION 2
#define GPS_IMPROVE     3
#define MAPPING         4
#define ERR             5

char states[6][20] = {"NO_GPS", "NO_FIX", "CHECK_PRECISION", "GPS_IMPROVE", "MAPPING", "ERROR"};
#define NB_STATES 6

//minimum hdop to accept GPS fix
#define MIN_HDOP 500

int fsm_state = NO_GPS;
unsigned long fsm_flag;

//Defines where on the server the points go, and 0 suspends the mapping.
//0 suspend mapping
//1 for mapping & data collection test
//2 for static hum&temp measures
//3-12 for static triangulation
//20 for data collection mapping production
//99 test to be discarded
int track_number = 20;

#define TRK_SUSPEND 0
#define TRK_TRIANG3 3
#define TRK_TRIANG4 4
#define TRK_TRIANG5 5
#define TRK_MAPPING 20

#define SECOND 1000
int t_check_fix = 5*SECOND;
int t_update_wait = 200;
int t_improve = SECOND;
int t_update = 500;
//the time to wait with registering after GPS fix
int t_precision = 30*SECOND; //ATTENTION: Overflow at 32'000
int t_lora_tx = 2*SECOND;

static const uint32_t GPSBaud = 4800;

char string[128];

int buttonPin = D4;
int humidityPin = D5;

// The TinyGPS++ object
TinyGPSPlus gps;

String oled_string;

// LoRa RX interrupt
bool data_received = false;

void loraRx() {
  data_received = true;
}

void setup() {
  // put your setup code here, to run once:

  //state machine setup
  fsm_flag = 0;

  //button
  pinMode(buttonPin,INPUT);
 
  String DevEui,NewDevEui;
  String AppEui,NewAppEui;
  String AppKey,NewAppKey;
  String loraClass,LoRaWANClass;

  String adr, dcs, dxrate, dr;

  byte join_status;
  int join_wait;

  Wire.begin();
  Serial.begin(GPSBaud);
  Serial.println("Starting");

  //humidity and temperature sensor
  dht.begin();

  // Init Oled
  SeeedOled.init();  //initialze SEEED OLED display

  SeeedOled.clearDisplay();         //clear the screen and set start position to top left corner
  SeeedOled.setNormalDisplay();     //Set display to normal mode
  SeeedOled.setHorizontalMode();    //Set addressing mode to Page Mode
  SeeedOled.setRotation(true);      //Modified SeeedOled library


  // GMX-LR init pass callback function
  gmxLR_init(&loraRx);

 
  // Set AppEui and AppKey
  // Uncomment these if you want to change the default keys
  
  // NewAppEui = "00:00:00:00:00:00:00:00";
  // NewAppKey = "6d:41:46:39:67:4e:30:56:46:4a:62:4c:67:30:58:33";

  // Region is available in GMX Firmware v2.0
  // available regions: EU868,US915,IN865,AS923,AU915,CN779,KR920
  // NewLoraRegion = "AS923";

  //Default class: A
  LoRaWANClass = "A";


#ifdef MULTIREGION
  gmxLR_getRegion(Region);
  if (NewLoraRegion.length() > 0 )
  {
    Region.trim();
    
    //Serial.println("**** UPDATING Region ****");
  
    if ( !Region.equals(NewLoraRegion) )
    {
       Serial.println("Setting Region:"+NewLoraRegion);
       gmxLR_setRegion(NewLoraRegion);
       // reboot GMX_LR1
       gmxLR_Reset();
    }
    else
    {
       Serial.println("Region is already:"+Region);
    }
  }
#endif

  gmxLR_getAppEui(AppEui);
  if (NewAppEui.length() > 0 )
  {
        AppEui.trim();
        
        Serial.println("**** UPDATING AppEUI ****");
        if ( !AppEui.equals(NewAppEui) )
        {
          Serial.println("Setting AppEui:"+NewAppEui);
          gmxLR_setAppEui(NewAppEui);
        }
        else
        {
          Serial.println("AppEui is already:"+AppEui);
        }
  }

  gmxLR_getAppKey(AppKey);
  if (NewAppKey.length() > 0 )
  {
      AppKey.trim();
      
      Serial.println("**** UPDATING AppKey ****");
      if ( !AppKey.equals(NewAppKey) )
      {
          Serial.println("Setting AppKey:"+NewAppKey);
          gmxLR_setAppKey(NewAppKey);
      }
      else
      {
          Serial.println("AppKey is already:"+AppKey);
      }
  }

  // Disable Duty Cycle  ONLY FOR DEBUG!
  gmxLR_setDutyCycle("0");


  // Set LoRaWAN Class
  gmxLR_setClass(LoRaWANClass);

  // Show LoRaWAN Params on OLED
  gmxLR_getDevEui(DevEui);
  gmxLR_getAppKey(AppKey);
  gmxLR_getAppEui(AppEui);
  displayLoraWanParams(DevEui, AppEui, AppKey);

  delay(2*SECOND);
  
  SeeedOled.clearDisplay();
  SeeedOled.setTextXY(0, 0);
  SeeedOled.putString("Joining...");

  Serial.println("Joining...");
  join_wait = 0;
  while ((join_status = gmxLR_isNetworkJoined()) != LORA_NETWORK_JOINED) {


    if ( join_wait == 0 )
    {
      Serial.println("LoRaWAN Params:");
      gmxLR_getDevEui(DevEui);
      Serial.println("DevEui:" + DevEui);
      gmxLR_getAppEui(AppEui);
      Serial.println("AppEui:" + AppEui);
      gmxLR_getAppKey(AppKey);
      Serial.println("AppKey:" + AppKey);
      gmxLR_getClass(loraClass);
      Serial.println("Class:" + loraClass);
      adr = String( gmxLR_getADR() );
      Serial.println("ADR:" + adr);
      dcs = String( gmxLR_getDutyCycle() );
      Serial.println("DCS:" + dcs);
      gmxLR_getRX2DataRate(dxrate);
      Serial.println("RX2 DataRate:" + dxrate);

      gmxLR_Join();
    }

    SeeedOled.setTextXY(1, 0);
    sprintf(string, "Attempt: %d", join_wait);
    SeeedOled.putString(string);

    join_wait++;

    if (!( join_wait % 100 )) {
      gmxLR_Reset();
      join_wait = 0;
    }

    delay(3*SECOND);

  };

  SeeedOled.setTextXY(2, 0);
  SeeedOled.putString("Joined!");

  //Disable ADR
  gmxLR_setADR("0");
  Serial.print("ADR: ");
  Serial.println(gmxLR_getADR());

  //Set SF, doesn't work. Ask Massimo!
  dr = String(gmxLR_setSF(String(LORA_SF10)));
  Serial.print("SF Data rate: ");
  Serial.println(dr);
  
  Serial.print("TXpow: ");
  String answ = "";
  gmxLR_getTXPower(answ);
  Serial.print(answ);

  delay(2*SECOND);
  SeeedOled.clearDisplay();
  oled_string = "                                                  ";

  oledPutState();

}

//put current state to OLED
void oledPutState(){
  SeeedOled.clearDisplay();
  SeeedOled.setTextXY(1, 0);
  SeeedOled.putString(states[fsm_state]);
  oledPutInfo();
}

void oledPutInfo(){
  SeeedOled.setTextXY(5, 0);
  SeeedOled.putString("Micha Burger");
  SeeedOled.setTextXY(6, 0);
  sprintf(string, "v1 03/2018   t%d",track_number);
  SeeedOled.putString(string);
}

//Put more information to the display
void oledPut(int line, char *str){
  SeeedOled.setTextXY(line,0);
  SeeedOled.putString(str);
}

//Send packet with LoRa
void sendLoraTX(){

  char lora_data[128];
  byte tx_buf[128];
  String tx_data;

  int temperature_int;
  int hum_int;
  long gps_lat = 0.0 * 10000000;
  long gps_lon = 0.0 * 10000000;
  
  //Read temperature and humidity
  temperature_int = dht.readTemperature(false) * 100; //false for celsius
  hum_int = dht.readHumidity() * 100;
           
  //get GPS data
  gps_lat = gps.location.lat()*10000000;
  gps_lon = gps.location.lng()*10000000;
  int nbSat = gps.satellites.value();
  if(nbSat>255) nbSat=255;
  int gpsHDOP = gps.hdop.value();
           
  //create package
  tx_buf[0] = 0x02; // packet header - multiple data
  tx_buf[1] = (temperature_int & 0xff00 ) >> 8;
  tx_buf[2] = temperature_int & 0x00ff;
  tx_buf[3] = (hum_int & 0xff00 ) >> 8;
  tx_buf[4] = hum_int & 0x00ff;
  tx_buf[5] = (gps_lat & 0xff000000) >> 24;
  tx_buf[6] = (gps_lat & 0x00ff0000) >> 16;
  tx_buf[7] = (gps_lat & 0x0000ff00) >> 8;
  tx_buf[8] = gps_lat & 0x000000ff;
  tx_buf[9] = (gps_lon & 0xff000000) >> 24;
  tx_buf[10] = (gps_lon & 0x00ff0000) >> 16;
  tx_buf[11] = (gps_lon & 0x0000ff00) >> 8;
  tx_buf[12] = gps_lon & 0x000000ff;
  tx_buf[13] = nbSat & 0x00ff;
  tx_buf[14] = (gpsHDOP & 0xff00) >> 8;
  tx_buf[15] = gpsHDOP & 0x00ff;
  tx_buf[16] = track_number & 0x00ff;
           
  sprintf(lora_data, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6], tx_buf[7], tx_buf[8], tx_buf[9], tx_buf[10], tx_buf[11], tx_buf[12], tx_buf[13], tx_buf[14], tx_buf[15], tx_buf[16] );
           
   //displayLoraTX(true); 
   tx_data = String(lora_data);

   //Set SF, doesn't work. Ask Massimo!
   gmxLR_setSF(String(LORA_SF10));
      
   gmxLR_TXData(tx_data);
   //displayLoraTX(false);
}


void loop() {
    //IMPROVE: Get rid of delay() statements in state machine!!!
    //--> Do with timer & millis()

    //change suspend mode or track number
    if(digitalRead(buttonPin)){
      switch (track_number)
      {
        case TRK_MAPPING:
          track_number = TRK_TRIANG3;
          SeeedOled.clearDisplay();
          oledPutInfo();
          delay(SECOND);
          break;
        case TRK_TRIANG3:
          track_number = TRK_SUSPEND;
          SeeedOled.clearDisplay();
          oledPut(1,"SUSPEND");
          oledPutInfo();
          delay(SECOND);
          break;
        case TRK_SUSPEND:
          track_number = TRK_MAPPING;
          SeeedOled.clearDisplay();
          oledPutInfo();
          delay(SECOND);
          break;
      }
    }

    if(track_number) {
      //check if GPS is still connected
      while (Serial.available() > 0 && !digitalRead(buttonPin)) 
        if (gps.encode(Serial.read())){
          
        //check GPS fix
        if(fsm_state!=NO_GPS && (!gps.location.isValid() || !gps.time.isValid() || gps.satellites.value()<1))
          fsm_state = NO_FIX;
        
        oledPutState();
        
        switch (fsm_state)
        {
          case NO_GPS: 
            if (gps.charsProcessed() > 10)
              fsm_state = NO_FIX;
            
            if(gps.location.isValid() && gps.time.isValid() && gps.satellites.value()>0) 
              fsm_state = CHECK_PRECISION;
  
            delay(t_check_fix);
            break;
      
          case NO_FIX:
            if(gps.location.isValid() && gps.time.isValid() && gps.satellites.value()>0)
              fsm_state = CHECK_PRECISION;
         
            delay(t_check_fix);
            break;
      
          case CHECK_PRECISION:
            if(gps.hdop.value()<MIN_HDOP && gps.hdop.value()>0){
              //store time when the fix was accepted
              fsm_flag = millis();
              fsm_state = GPS_IMPROVE;
            }
            sprintf(string, "HDOP: %d", gps.hdop.value());
            oledPut(2,string);
            sprintf(string, "Satellites: %d",gps.satellites.value());
            oledPut(3,string);
            delay(t_check_fix);
            break;
      
          case GPS_IMPROVE:
            if(millis()-fsm_flag > t_precision) {
              fsm_flag = millis();
              fsm_state = MAPPING;
              oledPut(2,"Done, start mapping!");
            }
            else{
              sprintf(string, "Wait: %d", (int)(0.001 * (t_precision - (millis()-fsm_flag))));
              oledPut(2,string);
            }
            //continuously check fix and precision
            if(gps.hdop.value()>MIN_HDOP){
              //store time when the fix was accepted
              fsm_state = CHECK_PRECISION;
              oledPut(2,"GPS precision lost");
            }
            delay(t_improve);
            break;
      
          case MAPPING:
            //continuously check fix and precision
            if(gps.hdop.value()>MIN_HDOP){
              //store time when the fix was accepted
              fsm_state = CHECK_PRECISION;
              oledPut(2,"GPS precision lost");
            }
            
            if(gps.location.isUpdated()){
              sprintf(string, "Since: %d s", (int) (0.001 * (millis()-fsm_flag)));
              oledPut(2,string);
              sprintf(string, "HDOP: %d", gps.hdop.value());
              oledPut(3,string);
              sprintf(string, "Satellites: %d",gps.satellites.value());
              oledPut(4,string); 
              sendLoraTX();
              delay(t_lora_tx);
            }
            else {
              sprintf(string, "Since: %d s", (int) (0.001 * (millis()-fsm_flag)));
              oledPut(2,string);
              oledPut(3,"No update, wait...");
              delay(t_update);
            }
            break;
        }  
      }
    }
}
