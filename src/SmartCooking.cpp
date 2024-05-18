/*
 * Project Perry3000 Smart Cooker
 * Author: Kathryn Perry
 * Date: April 4, 2024
 * Description: This program automates the process of cooking a meal.  It takes as input an NFC Recipe Card or a command 
 *              from Adafruit and then goes through the process of cooking a meal. The cooking process consists of heating, 
 *              cooking, and cooling while directing the user at each step. For more information see the readme file
 */
#include "SmartCooking.h"

SYSTEM_THREAD(ENABLED);
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

  //Initiliaze oven relay
  pinMode(OVENRELAY, OUTPUT);
  digitalWrite(OVENRELAY, LOW); // Make sure Oven is off

  //Initialize hall sensor
  pinMode(HALLPIN, INPUT);

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
  myDFPlayer.volume(30);  //Set volume value. From 0 to 30
  playClip(8);

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

  // Setup MQTT subscription
  mqtt.subscribe(&smartCookerRemote);

  // Get data from Adafruit
   // new Thread("adafruitData", getAdafruitSubscription);

}

void loop () {

  if(onOffButton.isClicked()){
    Serial.printf("On/off button pressed: %i!!\n\n", buttonFlag);
    if(buttonFlag == LOW){
      buttonFlag = HIGH;
      status = READY;
     }else{
      buttonFlag = LOW;
      status = SHUTDOWN;
    }
  }

  MQTT_connect();
  MQTT_ping();

  getAdafruitSubscription(&status, &ci, &notificationFlag);

  switch(status){
    case READY:
      Serial.printf("System Ready\n\n");
      //Just sitting here waiting until we get a recipe
     if(!notificationFlag){
        pixelFill(0,PIXELCOUNT, green);
        displayNotification("System Ready");
        notificationFlag = true;
        // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
        }
        playClip(9);
      }

      if(nfcRead(&ci, &status, &notificationFlag)){
        // There has been an error need to scan card again
        playClip(6);
        playClip(7);
      }
      break;
    case SHUTDOWN:
      // Only come in here if we are going to sleep
      Serial.printf("Status is Shut Down\n");
      if(!notificationFlag){
        pixelFill(0, PIXELCOUNT, blue);
        displayNotification("Oven Off");
        notificationFlag = true;
      }

      /// Make sure the oven is off to be safe
      digitalWrite(OVENRELAY, LOW);
      sleepULP(status);
      status = READY;
      notificationFlag = false;
      break;
    case HEATING:
      Serial.printf("Oven Heating\n\n");
      if(!notificationFlag){
        pixelFill(0, PIXELCOUNT, yellow);
        displayNotification("Oven Heating");
        playClip(1);
        notificationFlag = true;
        digitalWrite(OVENRELAY, HIGH);
          // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
        }
        notificationFlag = true;
       }
      tempF = temperatureRead();
      if(tempF >= ci.cookTemp){
        status = WAITINGFORFOODIN;
        notificationFlag = false;
        waitTimer.startTimer(WAITTIME);
       }
      break;
    case WAITINGFORFOODIN:
      Serial.printf("Status is Waiting for Food In, temp: %f\n", tempF);
      // Want to keep displaying so we can visually monitor the temp if needed
      displayNotification("Put food in the oven", tempF);
      playClip(6);
      if(!notificationFlag){
        reminder++;
        pixelFill(0, PIXELCOUNT, orange);
        playClip(2);
          //  First time through Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
        }
        notificationFlag = true;
      }
      
      doorOpen = digitalRead(HALLPIN);

      if(!doorOpen) {
          notificationFlag = false;
          status = COOKING;
          cookTimer.startTimer(ci.cookTime - COOLINGTEMPTIME);  // Food is in the oven start cooking
          break;
      } else{
         if(waitTimer.isTimerReady()){
           if(reminder < NUMOFREMINDERS){
              playClip(2);
              reminder++;
              waitTimer.startTimer(WAITTIME);
            }
           else{
              status = SHUTDOWN;
              notificationFlag = false;
              reminder = 0;
            }
          }          
          tempF = temperatureRead();

          if(tempF >= (ci.cookTemp)){ // Need to keep monitoring the temp while waiting so oven doesn't get too hot
            digitalWrite(OVENRELAY, LOW);
          }
          else if(tempF < (ci.cookTemp - TEMPOFFSET)){
            digitalWrite(OVENRELAY, HIGH);
          }
      }
      break;
    case COOKING:
      Serial.printf("Status is Cooking, Temp: %0.2f\n", tempF);
      // We need to keep displaying notification so we can visually monitor the temp
      displayNotification("Food Cooking, Temp: ", tempF);
      if(!notificationFlag){
        pixelFill(0, PIXELCOUNT, red);
        playClip(3);
         // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
        }
        notificationFlag = true;
      }
      if(cookTimer.isTimerReady()){
        // Food is done cooking
        digitalWrite(OVENRELAY, LOW);
        coolTimer.startTimer(COOLINGTEMPTIME);
        status = COOLING;
        notificationFlag = false;
      }else{
        tempF = temperatureRead();
        if(tempF > (ci.cookTemp)){
          digitalWrite(OVENRELAY, LOW);
        }
        else if(tempF < (ci.cookTemp - TEMPOFFSET)){
          digitalWrite(OVENRELAY, HIGH);
        }
      }
      break;
    case COOLING:
      Serial.printf("Status is Cooling\n");
      if(!notificationFlag){
        pixelFill(0, PIXELCOUNT, indigo);
        reminder++;
        playClip(4);
        // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
        }

        displayNotification("Food is Cooling");
        notificationFlag = true;
      }

     if(coolTimer.isTimerReady()){
        status=WAITINGFORFOODOUT;
        notificationFlag = false;
        waitTimer.startTimer(WAITTIME);
      }
      break;
    case WAITINGFORFOODOUT:
      Serial.printf("Status is Waiting for Food Out\n");
      if(!notificationFlag){
        pixelFill(0, PIXELCOUNT, violet);
        displayNotification("Take Food Out of the Oven");
        playClip(6);
        playClip(5);
        reminder++;
        notificationFlag = true;
        // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
        }
      }
      playClip(6);
      doorOpen = digitalRead(HALLPIN);

      if(!doorOpen){
        // put the system to sleep
        status = SHUTDOWN;
        notificationFlag = false;
        reminder = 0;
        break;
      }if(waitTimer.isTimerReady()){
        if(reminder < NUMOFREMINDERS){
          reminder++;
          playClip(5);
          waitTimer.startTimer(WAITTIME);
        }else {
          // put the system to sleep
          status = SHUTDOWN;
          notificationFlag = false;
          reminder = 0;
        }
    } 
    break;
  }
}

// Lights up a segment of the pixel strip while randomly changing brightness for a blinking affect
void pixelFill(int startPixel, int endPixel, int hexColor, bool clear){
  if(clear){
    pixel.clear();
    pixel.show();
  }else{
    for(int i = startPixel; i < endPixel; i++){
      pixel.setPixelColor(i, hexColor);
      pixel.show();
    }
  }
}

// Displays notifications to OLED
void displayNotification(String message, float temp) {
  String dateTime, timeStamp;
  dateTime = Time.timeStr();
  display.clearDisplay();
  display.display();
  timeStamp = dateTime.substring(11, 19);
  display.setTextSize(TEXTSIZE);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  Serial.printf("timestamp: %s\n", timeStamp.c_str());
  if(temp==0){
    display.printf("%s\nTime: %s ", message.c_str(), timeStamp.c_str());

  }else{
   display.printf("%s %0.2f\nTime: %s ", message.c_str(),  temp, timeStamp.c_str());
  }
 display.display();
}

bool nfcRead(struct cookingInstructions* cookingStruct, systemStatus * status, bool *notification){
  if (nfc.scan()) {
    if (nfc.readData(dataNameRead, RECIPENAMEBLOCK) != 1) {
      Serial.printf("Block %i Read failure!\n", RECIPENAMEBLOCK);
      return true;
    }
    else {
      cookingStruct->recipeName = (char *)dataNameRead;
      Serial.printf("Recipe Name: %s\n", cookingStruct->recipeName.c_str());
    }
    if (nfc.readData(dataTempRead, RECIPETEMPBLOCK) != 1) {
       Serial.printf("Block %i Read failure!\n", RECIPETEMPBLOCK);
       return true;
    }
    else {
      cookingStruct->cookTemp = (atoi((char*)dataTempRead) - 150); // Had to subtract from temperature to compensate for a faulty thermocouple
      Serial.printf("Recipe Temp: %i\n", cookingStruct->cookTemp);
    }
    if (nfc.readData(dataTimeRead, RECIPETIMEBLOCK) != 1) {
      Serial.printf("Block %i Read failure!\n", RECIPETIMEBLOCK);
      return true;
   }
    else {
      cookingStruct->cookTime = atoi((char*)dataTimeRead) * 60000; // Need to convert to ms
      Serial.printf("Recipe Time: %i\n", cookingStruct->cookTime);
    }
    delay(500);

    *status=HEATING;
    *notification = false;

  }
  return false;
}

float temperatureRead(){
  float tempC, tempF;
  // Monitor Temperature of Oven
  tempStatus = thermocouple.read();
  if (tempStatus != STATUS_OK) {
    Serial.printf("temperatureRead ERROR!\n");
  }

  tempC = thermocouple.getTemperature();
  tempF = (9.0/5.0)* tempC + 32;
  Serial.printf("Temperature:%0.2f or %0.2f\n",tempC,tempF);
  return tempF;
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

void sleepULP(systemStatus status){
  displayNotification("System Turned Off");
  pixelFill(0, PIXELCOUNT, white, true);
  // Just to be safe
  digitalWrite(OVENRELAY, LOW);

  SystemSleepConfiguration config;
  config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(D11, CHANGE);
  SystemSleepResult result = System.sleep(config);
  //delay(1000);

  if(result.wakeupReason() == SystemSleepWakeupReason::BY_GPIO){
    Serial.printf("We just woke up\n");
    displayNotification("System Turned On");
    buttonFlag = true;
    playClip(8);
  }
}

void playClip(int trackNumber){
    myDFPlayer.play(trackNumber);
    delay(6000);
}

void getAdafruitSubscription(systemStatus *status, struct cookingInstructions* cookingStruct, bool *notification){
  Adafruit_MQTT_Subscribe *subscription;

  while ((subscription = mqtt.readSubscription(0))) {
    Serial.printf("getting subscription\n\n");
    if (subscription == &smartCookerRemote) {
      subValue = atoi((char *)smartCookerRemote.lastread);
      Serial.printf("cooker value: %d\n", subValue);
      switch(subValue){
        case DECVOL:
          Serial.printf("Decreasing Volumn\n");
          myDFPlayer.volumeDown();
          break;
        case INCVOL:
           Serial.printf("Increasing Volumn\n");
           myDFPlayer.volumeUp();
          break;
        case SLEEP:
           Serial.printf("Shutting Down\n");
           sleepULP(*status);
          break;
        case LASAGNA:
          Serial.printf("Cooking Lasagna\n");
          cookingStruct->recipeName = recipes[0].cookTime;
          cookingStruct->cookTemp = recipes[0].cookTemp;
          cookingStruct->cookTime = recipes[0].cookTime;
          *status=HEATING;
          *notification = false;          
          break;
        case CHICKEN:
         Serial.printf("Cooking Chicken\n");
          cookingStruct->recipeName = recipes[1].cookTime;
          cookingStruct->cookTemp = recipes[1].cookTemp;
          cookingStruct->cookTime = recipes[1].cookTime;
          *status=HEATING;
          *notification = false;          
          break;
        case MACCHEESE:
         Serial.printf("Cooking Mac & Cheese\n");
          cookingStruct->recipeName = recipes[2].cookTime;
          cookingStruct->cookTemp = recipes[2].cookTemp;
          cookingStruct->cookTime = recipes[2].cookTime;
          *status=HEATING;
          *notification = false;          
          break;
        case STEAK:
        Serial.printf("Cooking Steak\n");
          cookingStruct->recipeName = recipes[3].cookTime;
          cookingStruct->cookTemp = recipes[3].cookTemp;
          cookingStruct->cookTime = recipes[3].cookTime;
          *status=HEATING;
          *notification = false;          
          break;
        case TURKEY:
        Serial.printf("Cooking Turkey\n");
          cookingStruct->recipeName = recipes[4].cookTime;
          cookingStruct->cookTemp = recipes[4].cookTemp;
          cookingStruct->cookTime = recipes[4].cookTime;
          *status=HEATING;
          *notification = false;          
          break;
      }
    }
  }
}
