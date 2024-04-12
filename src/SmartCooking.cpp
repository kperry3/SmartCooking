/*
 * Project Perry3000 Smart Cooker
 * Author: Kathryn Perry
 * Date: April 4, 2024
 * Description:
 */

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
const int INTERRUPT = 1;
const int POLLING = 0;
const int RECIPENAMEBLOCK = 1;
const int RECIPETEMPBLOCK = 2;
const int RECIPETIMEBLOCK = 4;

/************Declare Functions*************/
void MQTT_connect();
bool MQTT_ping();
void getConc() ;
void pixelFill(int startPixel, int endPixel, int hexColor);
void displayNotification(String message);
void nfcRead(struct cookingInstructions* cookingStruct);
void nfcWrite();
void temperatureRead();
void wakeUpProcess();
void watchdogHandler();
void watchdogCheckin();

// Declare Objects
Timer timer(1000, watchdogCheckin);
DFRobot_PN532_IIC  nfc(PN532_IRQ, POLLING);
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_NeoPixel pixel(PIXELCOUNT, SPI1, WS2812B);
Button onOffButton(ONOFFBUTTON);
IoTTimer cookTimer, coolTimer, waitTimer;
MAX6675 thermocouple;
DFRobotDFPlayerMini myDFPlayer;
/************ Global State (you don't need to change this!) *** ***************/
TCPClient TheClient;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and
// login details.
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/
// Setup Feeds to publish or subscribe
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Subscribe manualFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/waterbutton");
Adafruit_MQTT_Publish aqFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/aqsensorfeed");
Adafruit_MQTT_Publish dustFeed =  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dustsensorfeed");
Adafruit_MQTT_Publish tempFeed =  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/bme289temp");
Adafruit_MQTT_Publish humidityFeed =  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidityfeed");
Adafruit_MQTT_Publish moistureFeed =  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moisturefeed");

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

// Variables
struct cookingInstructions ci;
ApplicationWatchdog *wd;
bool tempToHigh = FALSE;
uint8_t tempStatus;
systemStatus status = SHUTDOWN;
int reminder = 0;
float tempC, tempF;
uint8_t dataRead[16] = {0};
uint8_t dataWrite[BLOCK_SIZE] = {"Hello World !"};
String message;
String dateTime, timeStamp;
int watchdogTimer = 2*3600000;  // Every two hours
uint8_t dataNameRead[16] = {0};
uint8_t dataTempRead[16] = {0};
uint8_t dataTimeRead[16] = {0};
uint8_t dataWriteName[BLOCK_SIZE] = {"Mac & Cheese"};
uint8_t dataWriteTemp[BLOCK_SIZE] = {"350"};
uint8_t dataWriteTime[BLOCK_SIZE] = {"40"};

int vol;

SYSTEM_MODE(AUTOMATIC);

void setup () {

  // sync time
  Time.zone(TIMEZONE);
  Particle.syncTime();

  // Connect to the internet
  WiFi.on();
  WiFi.connect();
  while (WiFi.connecting()) {
      Serial.printf(".");
  }
  Serial.printf("\n\n\n");

  // Initialize serial port for debugging purposes
  Serial.begin(9600);
  Serial1.begin(9600);
  waitFor(Serial.isConnected,10000);

  //Initialize thermocouple
  thermocouple.begin(SCK, SS, MISO);

  //Initiliaze relay
  pinMode(OVENRELAY, OUTPUT);
  digitalWrite(OVENRELAY, LOW); // Make sure Oven is off

  //Initialize mp3 player
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!myDFPlayer.begin(Serial1)) {  //Use serial1 to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0);
    }
  }
  Serial.println(F("DFPlayer Mini online."));

  myDFPlayer.volume(20);  //Set volume value. From 0 to 30
  
  //Initialize NFC controller
  Serial.print("Initializing NFC Controller");
  while (!nfc.begin()) {
    Serial.print(".");
    delay (1000);
  }
  Serial.println();
  Serial.println("Waiting for a card......");

  // Initialize Neopixels
  pixel.begin();
  pixel.setBrightness(BRIGHTNESS);
  pixel.show();

  // Initialize Display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //display.setRotation(2); ??? MAY HAVE TO ROTATE
  display.setRotation(0);
  display.clearDisplay();
  display.display();

  // Setup watchdog timer
  //wd = new ApplicationWatchdog(watchdogTimer, watchdogHandler , 1536);
}

void loop () {
  if(onOffButton.isPressed()){ //NEED TO TURN ON AND OFF
    Serial.printf("On/off button pressed!!\n\n");

   
    status = SHUTDOWN;
  }

  // Monitor Status
  switch(status){
    case SHUTDOWN:
     // Set leds to blue and display timestamp and message "System OFF"  Send status/email to adafruit
      pixelFill(0, PIXELCOUNT, blue);
      displayNotification("System Off");
      digitalWrite(OVENRELAY, LOW); // shut off oven

      // tempToHigh = false -> go to sleep
     /*  if(tempToHigh == false){
        sleepULP();
      } */
     // nfcRead(&ci);
      vol = myDFPlayer.readVolume();
     Serial.printf("volumn: %i\n", vol);
    myDFPlayer.play(1);
    delay(6000);
  
  
      Serial.printf("Status is Shut Down\n");

      break;
    case HEATING:
       // Set leds to Yellow and display timestamp and message "Oven heating"  Send status/email to adafruit

    // Check temp -> at temp -> status = waiting for food, tell user put food in oven, set reminder timer increase reminder

      Serial.printf("Status is Heating\n");
      break;
    case WAITINGFORFOODIN:
       // Set leds to orange and display timestamp and message "Put Food IN"  Send status/email to adafruit

    // drop in temp (person put food in oven??) -> status=cooking -> set cooking timer -> tell user cooking ->reset reminder
    // No drop in temp -> is reminder timing up?
                    // reminder timer up -> reminder < 3 -> tell user to put food in oven -> increase reminder
                    // reminder timer up -> reminder !< 3 status = shut down -> TemToHigh = F, reset reminder
       Serial.printf("Status is Waiting for Food In\n");
      break;
    case COOKING:
       // Set leds to red and display timestamp and message "Food Cooking"  Send status/email to adafruit

    //  Cook time up -> status = cooling -> tell user cooling -> set cooling timer -> increase reminder -> shut off oven
    // cook time not up -> temp too high -> status = shut down -> tempToHigh = T

      Serial.printf("Status is Cooking\n");
       break;
    case COOLING:
          // Set leds to indigo and display timestamp and message "Food Cooling"  Send status/email to adafruit

    // Cooling timer up -> tell user take food out of oven -> status = food out -> start wait timer -> increaser reminder
      Serial.printf("Status is Cooling\n");
       break;
    case WAITINGFORFOODOUT:
    // Set leds to violet and display timestamp and message "Take Food OUT"  Send status/email to adafruit

    // temp change?? (took food out)  -> status = shut down -> reset reminder -> tempToHigh=F
    // No temp change -> wait timer up -> reminder < 3 -> increase reminder -> tell user take food out
    // No temp change -> wait timer up -> reminder >= 3 -> status - shut down -> reset reminder -> tempToHigh=F
      Serial.printf("Status is Waiting for Food Out\n");
      break;
  }
}

// Lights up a segment of the pixel strip while randomly changing brightness for a blinking affect
void pixelFill(int startPixel, int endPixel, int hexColor){
  // Only want blinking if there is an issue
  if((hexColor != blue) && (hexColor != green) && (hexColor != purple)){
    pixel.setBrightness(random(1, 255));
  }

  for(int i = startPixel; i < endPixel; i++){
    pixel.setPixelColor(i, hexColor);
    pixel.show();
  }
}

// Displays notifications to OLED
void displayNotification(String message) {
  display.setTextSize(TEXTSIZE);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.printf("%s\n", message.c_str());
  display.display();
}

void nfcRead(struct cookingInstructions* cookingStruct){
  if (nfc.scan()) {
    if (nfc.readData(dataNameRead, RECIPENAMEBLOCK) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPENAMEBLOCK);
      Serial.println(" read failure!");
    }
    else {
      cookingStruct->recipeName = (char *)dataNameRead;
      Serial.printf("Recipe Name: %s\n", cookingStruct->recipeName.c_str());
    }
    if (nfc.readData(dataTempRead, RECIPETEMPBLOCK) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPETEMPBLOCK);
      Serial.println(" read failure!");
    }
    else {
      cookingStruct->cookTemp = atoi((char*)dataTempRead);
      Serial.printf("Recipe Temp: %i\n", cookingStruct->cookTemp);
    }
    if (nfc.readData(dataTimeRead, RECIPETIMEBLOCK) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPETIMEBLOCK);
      Serial.println(" read failure!");
    }
    else {
      cookingStruct->cookTime = atoi((char*)dataTempRead);
      Serial.printf("Recipe Time: %i\n", cookingStruct->cookTime);
    }
    delay(500);
  }  
  
}

void nfcWrite(){
 if (nfc.scan()) {
    if (nfc.writeData(RECIPENAMEBLOCK, dataWriteName) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPENAMEBLOCK);
      Serial.println(" write failure!");
    }
    else {
      Serial.print("Block ");
      Serial.print(RECIPENAMEBLOCK);
      Serial.println(" write success!");
      Serial.print("Data written(string):");
      Serial.println((char *)dataWriteName);
      Serial.print("Data written(HEX):");
      for (int i = 0; i < BLOCK_SIZE; i++) {
        Serial.print(dataWriteName[i], HEX);
        Serial.print(" ");
      }
    }
    if (nfc.writeData(RECIPETEMPBLOCK, dataWriteTemp) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPETEMPBLOCK);
      Serial.println(" write failure!");
    }
    else {
      Serial.print("Block ");
      Serial.print(RECIPETEMPBLOCK);
      Serial.println(" write success!");
      Serial.print("Data written(string):");
      Serial.println((char *)dataWriteTemp);
      Serial.print("Data written(HEX):");
      for (int i = 0; i < BLOCK_SIZE; i++) {
        Serial.print(dataWriteTemp[i], HEX);
        Serial.print(" ");
      }
    }
    if (nfc.writeData(RECIPETIMEBLOCK, dataWriteTime) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPETIMEBLOCK);
      Serial.println(" write failure!");
    }
    else {
      Serial.print("Block ");
      Serial.print(RECIPETIMEBLOCK);
      Serial.println(" write success!");
      Serial.print("Data written(string):");
      Serial.println((char *)dataWriteTime);
      Serial.print("Data written(HEX):");
      for (int i = 0; i < BLOCK_SIZE; i++) {
        Serial.print(dataWriteTime[i], HEX);
        Serial.print(" ");
      }
    }
 }
  delay(2000);
}

void temperatureRead(){
   // Monitor Temperature of Oven
  tempStatus = thermocouple.read();
  if (tempStatus != STATUS_OK) {
    Serial.printf("ERROR!\n");
  }

  tempC = thermocouple.getTemperature();
  tempF = (9.0/5.0)* tempC + 32;
  Serial.printf("Temperature:%0.2f or %0.2f\n",tempC,tempF);
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
    int8_t ret;

    // Return if already connected.
    if (mqtt.connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");

    while ((ret = mqtt.connect()) !=  0) {  // connect will return 0 for connected
        Serial.printf("Error Code %s\n", mqtt.connectErrorString(ret));
        Serial.printf("Retrying MQTT connection in 5 seconds...\n");
        mqtt.disconnect();
        delay(5000);  // wait 5 seconds and try again
    }
    Serial.printf("MQTT Connected!\n");
}

bool MQTT_ping() {
    static unsigned int last;
    bool pingStatus;

    if ((millis() - last) > 120000) {
        Serial.printf("Pinging MQTT \n");
        pingStatus = mqtt.ping();
        if (!pingStatus) {
            Serial.printf("Disconnecting \n");
            mqtt.disconnect();
        }
        last = millis();
    }
    return pingStatus;
}

void sleepULP(){
  SystemSleepConfiguration config;

  // Wake up when NFC scanned, when remote cooking activated, or after 2 hours
  config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(D0, RISING).network(NETWORK_INTERFACE_WIFI_AP).duration(watchdogTimer);
  SystemSleepResult result = System.sleep(config);
  delay(1000);

  if(result.wakeupReason() == SystemSleepWakeupReason::BY_GPIO){
    Serial.printf("Just woke up Recipe Card Scanned\n");
    wakeUpProcess();

  }
  if(result.wakeupReason() == SystemSleepWakeupReason::BY_NETWORK){
    Serial.printf("Just woke up Remote Cooking\n");

  }
  if(result.wakeupReason() == SystemSleepWakeupReason::BY_RTC){
    // We want to send a pulse???
    Serial.printf("Watchdog - Time to check in\n");
    watchdogCheckin();
  }
}

void wakeUpProcess(){
  // Read nfc into struct -> turn on oven -> set status= heating -> tell user wait for oven to heat up
  // Set leds to green and display timestamp and message "System ON"
  dateTime = Time.timeStr();
  timeStamp = dateTime.substring(11, 19);

  // Send wakeup timestamp and message to user so they know person is cooking



}

void watchdogHandler() {
  System.reset(RESET_NO_WAIT);
}

void watchdogCheckin(){
  ApplicationWatchdog::checkin();
}
