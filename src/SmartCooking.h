#include "Particle.h"
#include "DFRobot_PN532.h"
#include "DFRobotDFPlayerMini.h"
#include "MAX6675.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT.h"
#include "neopixel.h"
#include "Button.h"
#include "IoTTimer.h"
#include "Colors.h"
#include "credentials.h"

const int TIMEZONE = -4;
const int ONOFFBUTTON = D11;
const int OVENRELAY = D4;
const int OLED_RESET= -1;
const int TEXTSIZE = 1;
const int PIXELCOUNT = 12;
const int BRIGHTNESS = 35;
const int BLOCK_SIZE = 16;
const int PN532_IRQ = 2;
const int POLLING = 0;
const int RECIPENAMEBLOCK = 1;
const int RECIPETEMPBLOCK = 2;
const int RECIPETIMEBLOCK = 4;
const int WATCHDOGTIMER = 300000; //120*60000;  // Every two hours
const int WAITTIME = 20000;//10*60000; //Remind every 10 minutes
const int COOLINGTEMPTIME = 5000;//15*6000;  // Has to be in ms
const int NUMOFREMINDERS = 3; // Three reminders before the system shuts down
const int TEMPOFFSET = 5; // NOT sure about this need to test

// Declare Objects
//Timer timer(1000, watchdogCheckin);
DFRobot_PN532_IIC  nfc(PN532_IRQ, POLLING);
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_NeoPixel pixel(PIXELCOUNT, SPI1, WS2812B);
Button onOffButton(ONOFFBUTTON);
IoTTimer cookTimer, coolTimer, waitTimer;
MAX6675 thermocouple;
DFRobotDFPlayerMini myDFPlayer;
TCPClient TheClient;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and
// login details.
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/
// Setup Feeds to publish or subscribe
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Subscribe smartCookerRemote = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/smartcooker");
Adafruit_MQTT_Publish smartCookerStatus = Adafruit_MQTT_Publish(&mqtt,AIO_USERNAME "/feeds/smartcookerstatus");

struct cookingInstructions {
  String recipeName;
  int cookTemp;
  int cookTime;
};

enum systemStatus {
  SHUTDOWN = 27,
  HEATING,
  WAITINGFORFOODIN,
  COOKING,
  COOLING,
  WAITINGFORFOODOUT
};

enum remoteControl {
  DECVOL = 0,
  INCVOL = 2,
  SLEEP = 6,
  LASAGNA = 16,
  CHICKEN = 17,
  MACCHEESE = 18,
  STEAK = 20,
  TURKEY = 21
};

// Variables
struct cookingInstructions ci;
ApplicationWatchdog *wd;
bool tempToHigh = TRUE;
uint8_t tempStatus;
systemStatus status = SHUTDOWN;
systemStatus previousStatus = SHUTDOWN;
int reminder = 0;
float tempC, tempF;
String message;
uint8_t dataNameRead[16] = {0};
uint8_t dataTempRead[16] = {0};
uint8_t dataTimeRead[16] = {0};
uint8_t dataWriteName[BLOCK_SIZE] = {"Mac & Cheese"};
uint8_t dataWriteTemp[BLOCK_SIZE] = {"350"};
uint8_t dataWriteTime[BLOCK_SIZE] = {"40"};
int vol;
int subValue;
int buttonFlag = HIGH;

/************Declare Functions*************/
void sleepULP(systemStatus status);
void MQTT_connect();
bool MQTT_ping();
void getConc() ;
void pixelFill(int startPixel, int endPixel, int hexColor, bool clear=false);
void displayNotification(String message, float temp=0);
bool nfcRead(struct cookingInstructions* cookingStruct);
void nfcWrite();
float temperatureRead();
void watchdogHandler();
void watchdogCheckin();
void playClip(int trackNumber);
void nfcHandler();
void getAdafruitSubscription(systemStatus status);