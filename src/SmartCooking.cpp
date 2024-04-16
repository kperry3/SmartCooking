/*
 * Project Perry3000 Smart Cooker
 * Author: Kathryn Perry
 * Date: April 4, 2024
 * Description:
 */
#include "SmartCooking.h"

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

  getAdafruitSubscription(status, &ci);

  switch(status){
    case READY:
      //Just sitting here waiting until we get a recipe
      pixelFill(0,PIXELCOUNT, green);
      if(!notificationFlag){
        displayNotification("System Ready");
        notificationFlag = true;
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
      pixelFill(0, PIXELCOUNT, blue);
      if(!notificationFlag){
        displayNotification("Oven Off");
        notificationFlag = true;
      }

      //Make sure the oven is off to be safe
      digitalWrite(OVENRELAY, LOW);
      sleepULP(status);
      status = READY;
      notificationFlag = false;
      break;
    case HEATING:
      if(!notificationFlag){
        displayNotification("Oven Heating");
        notificationFlag = true;
        digitalWrite(OVENRELAY, HIGH);
          // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
          previousStatus = status;
        }
       }
      Serial.printf("Oven Heating\n\n");
      // Turn on oven
      pixelFill(0, PIXELCOUNT, yellow);

      tempF = temperatureRead();
      if(tempF >= ci.cookTemp){
        status = WAITINGFORFOODIN;
        notificationFlag = false;
        reminder++;
        waitTimer.startTimer(WAITTIME);
       playClip(6);
       playClip(2);
      }
      break;
    case WAITINGFORFOODIN:
      playClip(6);
      pixelFill(0, PIXELCOUNT, orange);
      if(!notificationFlag){
          //  First time through Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
          previousStatus = status;
        }
      }
      displayNotification("Put food in the oven", tempF);
      Serial.printf("Temp: %0.2f\n",  tempF);
      doorOpen = digitalRead(HALLPIN);
      Serial.printf("Door open value: %d\n", doorOpen);

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

      if(!doorOpen) {
          notificationFlag = false;
          status = COOKING;
          cookTimer.startTimer(ci.cookTime - COOLINGTEMPTIME);  // Food is in the oven start cooking
      } else{
          if(tempF >= (ci.cookTemp + TEMPOFFSET)){ // Need to keep monitoring the temp while waiting so oven doesn't get too hot
            digitalWrite(OVENRELAY, LOW);
          }
          else if(tempF < (ci.cookTemp - TEMPOFFSET)){
            digitalWrite(OVENRELAY, HIGH);
          }
      }
      Serial.printf("Status is Waiting for Food In\n");
      break;
    case COOKING:
      pixelFill(0, PIXELCOUNT, red);
      // We need to keep displaying notification so we can visually monitor the temp
      displayNotification("Food Cooking, Temp: ", tempF);
      if(!notificationFlag){
        // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
          previousStatus = status;
        }

      }
      if(cookTimer.isTimerReady()){
        // Food is done cooking
        digitalWrite(OVENRELAY, LOW);
        reminder++;
        playClip(5);
        coolTimer.startTimer(COOLINGTEMPTIME);
        status = COOLING;
        notificationFlag = false;
      }else{
        tempF = temperatureRead();
        if(tempF > (ci.cookTemp + TEMPOFFSET)){
          digitalWrite(OVENRELAY, LOW);
        }
        else if(tempF < (ci.cookTemp - TEMPOFFSET)){
          digitalWrite(OVENRELAY, HIGH);
        }
      }
      Serial.printf("Status is Cooking, Temp: %0.2f\n", tempF);
       break;
    case COOLING:
      pixelFill(0, PIXELCOUNT, indigo);
      if(!notificationFlag){
                  // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
          previousStatus = status;
        }

        displayNotification("Food is Cooling");
        notificationFlag = true;
      }
     if(coolTimer.isTimerReady()){
        reminder++;
        status=WAITINGFORFOODOUT;
        notificationFlag = false;
        waitTimer.startTimer(WAITTIME);
      }

       Serial.printf("Status is Cooling\n");
       break;
    case WAITINGFORFOODOUT:
      pixelFill(0, PIXELCOUNT, violet);
      if(!notificationFlag){
        displayNotification("Take Food Out of the Oven");
        notificationFlag = true;
        // Send to adafruit
        if(mqtt.Update()) {
          smartCookerStatus.publish(status);
          previousStatus = status;
        }
      }
      playClip(6);
      doorOpen = digitalRead(HALLPIN);

    if(waitTimer.isTimerReady()){
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
    }else if(doorOpen){
        // put the system to sleep
        status = SHUTDOWN;
        notificationFlag = false;
        reminder = 0;
     }

      Serial.printf("Status is Waiting for Food Out\n");
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
      cookingStruct->cookTemp = atoi((char*)dataTempRead);
      Serial.printf("Recipe Temp: %i\n", cookingStruct->cookTemp);
    }
    if (nfc.readData(dataTimeRead, RECIPETIMEBLOCK) != 1) {
      Serial.printf("Block %i Read failure!\n", RECIPETIMEBLOCK);
      return true;
   }
    else {
      cookingStruct->cookTime = atoi((char*)dataTimeRead) * 6000; // Need to convert to ms
      Serial.printf("Recipe Time: %i\n", cookingStruct->cookTime);
    }
    delay(500);

    *status=HEATING;
    *notification = false;
    playClip(1);

    // TESTING NEED TO DELETE THIS
    cookingStruct->cookTime = 102000; // Cook for 5 minutes
    cookingStruct->cookTemp = 320;
  
  }
  return false;
}

float temperatureRead(){
  float tempC, tempF;
  // Monitor Temperature of Oven
  tempStatus = thermocouple.read();
  if (tempStatus != STATUS_OK) {
    Serial.printf("ERROR!\n");
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

  SystemSleepConfiguration config;
  config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(D11, CHANGE);
  SystemSleepResult result = System.sleep(config);
  delay(1000);

  if(result.wakeupReason() == SystemSleepWakeupReason::BY_GPIO){
    Serial.printf("We just woke up\n");
    displayNotification("System Turned On");
    buttonFlag = true;
    playClip(8);
  }
}

void playClip(int trackNumber){
  Serial.printf("read state: %i\n",myDFPlayer.readState());
    myDFPlayer.play(trackNumber);
    delay(6000);
}

void getAdafruitSubscription(systemStatus status, struct cookingInstructions* cookingStruct){
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
           sleepULP(status);
          break;
        case LASAGNA:
          Serial.printf("Cooking Lasagna\n");
          cookingStruct->recipeName = recipes[0].cookTime;
          cookingStruct->cookTemp = recipes[0].cookTemp;
          cookingStruct->cookTime = recipes[0].cookTime;
          break;
        case CHICKEN:
         Serial.printf("Cooking Chicken\n");
          cookingStruct->recipeName = recipes[1].cookTime;
          cookingStruct->cookTemp = recipes[1].cookTemp;
          cookingStruct->cookTime = recipes[1].cookTime;
          break;
        case MACCHEESE:
         Serial.printf("Cooking Mac & Cheese\n");
          cookingStruct->recipeName = recipes[2].cookTime;
          cookingStruct->cookTemp = recipes[2].cookTemp;
          cookingStruct->cookTime = recipes[2].cookTime;
           break;
        case STEAK:
        Serial.printf("Cooking Steak\n");
          cookingStruct->recipeName = recipes[3].cookTime;
          cookingStruct->cookTemp = recipes[3].cookTemp;
          cookingStruct->cookTime = recipes[3].cookTime;
          break;
        case TURKEY:
        Serial.printf("Cooking Turkey\n");
          cookingStruct->recipeName = recipes[4].cookTime;
          cookingStruct->cookTemp = recipes[4].cookTemp;
          cookingStruct->cookTime = recipes[4].cookTime;
          break;
      }
    }
  }
}
