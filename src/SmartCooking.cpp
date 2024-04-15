/*
 * Project Perry3000 Smart Cooker
 * Author: Kathryn Perry
 * Date: April 4, 2024
 * Description:
 */
#include "SmartCooking.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(AUTOMATIC);

void setup () {

  // Connect to the internet
  WiFi.on();
  WiFi.connect();
  while (WiFi.connecting()) {
      Serial.printf(".");
  }
  Serial.printf("\n\n\n");

  // sync time
  Time.zone(TIMEZONE);
  Particle.syncTime();

  // Setup watchdog timer - NOT working when we go to sleep
 // wd = new ApplicationWatchdog(60000, watchdogHandler , 1536);

  // Initialize serial port for debugging purposes
  Serial.begin(9600);
  Serial1.begin(9600);
  waitFor(Serial.isConnected,10000);

  //Initialize thermocouple
  thermocouple.begin(SCK, SS, MISO);

  //Initiliaze oven relay
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

  myDFPlayer.volume(30);  //Set volume value. From 0 to 30

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
     }else{
      buttonFlag = LOW;
    }
    tempToHigh = false;
    status = SHUTDOWN;
  }

  if(nfcRead(&ci)){
    // There has been an error need to scan card again
    playClip(6);
    playClip(7);
  }

  MQTT_connect();
  MQTT_ping();

  getAdafruitSubscription(status);

  switch(status){
    case SHUTDOWN:
     
      Serial.printf("Status is Shut Down\n"); 
      pixelFill(0, PIXELCOUNT, blue);
      displayNotification("System Off");

      if(tempToHigh == false && buttonFlag == LOW){
        digitalWrite(OVENRELAY, LOW);
        sleepULP(status);
      }else if(tempToHigh == true){
        digitalWrite(OVENRELAY, LOW);
      }
      // monitor temp only want to monitor if previous status is waiting for food in or cooking
      if(previousStatus == COOKING || previousStatus == WAITINGFORFOODIN){
         tempF = temperatureRead();
         if(tempF <= ci.cookTemp + TEMPOFFSET){
         status = previousStatus;
      }
    }
      break;
    case HEATING:
      displayNotification("Oven Heating");
      Serial.printf("Oven Heating\n\n");
      // Turn on oven
      digitalWrite(OVENRELAY, HIGH);
      pixelFill(0, PIXELCOUNT, yellow);
       //   Send status/email to adafruit  NOT GOING TO WORK NEED TO CHANGE tHIS!!
      if(previousStatus != status){
          if(mqtt.Update()) {
           smartCookerStatus.publish(status);
           previousStatus = status;
          }   
       }
      tempF = temperatureRead();
      if(tempF >= ci.cookTemp){
        previousStatus = COOKING;
        status = WAITINGFORFOODIN;
        reminder++;
        waitTimer.startTimer(WAITTIME);
        playClip(2);
      }
      break;
    case WAITINGFORFOODIN:
      playClip(6);
      pixelFill(0, PIXELCOUNT, orange);
      displayNotification("Put food in the oven", tempF);
         Serial.printf("Temp: %0.2f\n",  tempF);
      if(waitTimer.isTimerReady()){
        if(reminder <= NUMOFREMINDERS){
          reminder++;
          playClip(2);
          waitTimer.startTimer(WAITTIME);
        }
        else{
          status = SHUTDOWN;
          tempToHigh = false;
          buttonFlag = LOW;
          reminder = 0;
        }
      } else {  
        tempF = temperatureRead();
        if(tempF > ci.cookTemp + TEMPOFFSET){
          previousStatus = WAITINGFORFOODIN;
          status = SHUTDOWN;
          tempToHigh = true;
        }else if(tempF <= ci.cookTemp){
          // Person put the food in the oven??? start cooking timer
          status = COOKING;
          cookTimer.startTimer(ci.cookTime - COOLINGTEMPTIME);
        }
      }

       //  First time through Send status/email to adafruit
      Serial.printf("Status is Waiting for Food In\n");
      break;
    case COOKING:
      pixelFill(0, PIXELCOUNT, red);

      displayNotification("Food Cooking, Temp: ", tempF);
       //  Send status/email to adafruit
      if(cookTimer.isTimerReady()){
        // Food is done cooking
        digitalWrite(OVENRELAY, LOW);
        reminder++;
        playClip(5);
        waitTimer.startTimer(WAITTIME);
        status = COOLING;
        previousStatus = SHUTDOWN;  //  We don't need this anymore so set back to default setting.????
      }else{
        tempF = temperatureRead();
        if(tempF > ci.cookTemp + TEMPOFFSET){
        previousStatus = COOKING;
        status = SHUTDOWN;
        tempToHigh = true;
        }         
      }
      Serial.printf("Status is Cooking\n");
       break;
    case COOLING:
      pixelFill(0, PIXELCOUNT, indigo);
      displayNotification("Food is Cooling");
          // Send status/email to adafruit
      if(coolTimer.isTimerReady()){
        reminder++;
        status=WAITINGFORFOODOUT;
        waitTimer.startTimer(WAITTIME);
      }

    // Send status to adafruit
      Serial.printf("Status is Cooling\n");
       break;
    case WAITINGFORFOODOUT:
      pixelFill(0, PIXELCOUNT, violet);
      displayNotification("Take Food Out of the Oven");
      playClip(6);

    // Send status/email to adafruit
    if(waitTimer.isTimerReady()){
      if(reminder <= NUMOFREMINDERS){
        reminder++;
        playClip(5);
        waitTimer.startTimer(WAITTIME);
      }else{
        // put the system to sleep
        tempToHigh = false;
        buttonFlag = LOW;
        status = SHUTDOWN;
     }
    }else{
        tempF = temperatureRead();
        if(tempF < ci.cookTemp - TEMPOFFSET){ // Temp taken out of oven???
          status = SHUTDOWN;
     }         
    }
    // Send status to adafruit
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
  if(temp=0){
    display.printf("%s\nTime: %s ", message.c_str(), timeStamp.c_str());
 
  }else{
   display.printf("%s %0.2f\nTime: %s ", message.c_str(),  temp, timeStamp.c_str());
  }
 display.display();
}

bool nfcRead(struct cookingInstructions* cookingStruct){
  if (nfc.scan()) {
    if (nfc.readData(dataNameRead, RECIPENAMEBLOCK) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPENAMEBLOCK);
      Serial.println("Read failure!");
      Serial.printf("Recipe Name: %s\n", cookingStruct->recipeName.c_str());
      return true;
    }
    else {
      cookingStruct->recipeName = (char *)dataNameRead;
      Serial.printf("Recipe Name: %s\n", cookingStruct->recipeName.c_str());
    }
    if (nfc.readData(dataTempRead, RECIPETEMPBLOCK) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPETEMPBLOCK);
      Serial.println("Read failure!");
      Serial.printf("Recipe Temp: %i\n", cookingStruct->cookTemp);
      return true;
    }
    else {
      cookingStruct->cookTemp = atoi((char*)dataTempRead);
      Serial.printf("Recipe Temp: %i\n", cookingStruct->cookTemp);
    }
    if (nfc.readData(dataTimeRead, RECIPETIMEBLOCK) != 1) {
      Serial.print("Block ");
      Serial.print(RECIPETIMEBLOCK);
      Serial.println("Read failure!");
       Serial.printf("Recipe Time: %i\n", cookingStruct->cookTime);
      return true;
   }
    else {
      cookingStruct->cookTime = atoi((char*)dataTimeRead) * 6000; // Need to convert to ms
      Serial.printf("Recipe Time: %i\n", cookingStruct->cookTime);
    }
    delay(500);
    status=HEATING;
    playClip(1);
    cookingStruct->cookTemp = 100;  // TESTING NEED TO DELETE THIS
    cookingStruct->cookTime = .3;
  }  
  return false;
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
  if(previousStatus != status){
    if(mqtt.Update()) {
      smartCookerStatus.publish(status);
    }   
  }

  SystemSleepConfiguration config;
  config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(D11, CHANGE);
  SystemSleepResult result = System.sleep(config);
  delay(1000);

  if(result.wakeupReason() == SystemSleepWakeupReason::BY_GPIO){
    Serial.printf("We just woke up\n");
    displayNotification("System Turned On");
    buttonFlag = true;
  }
}

void watchdogHandler() {
  System.reset(RESET_NO_WAIT);
}

void watchdogCheckin(){
  ApplicationWatchdog::checkin();
}

void playClip(int trackNumber){  
  Serial.printf("read state: %i\n",myDFPlayer.readState());
    myDFPlayer.play(trackNumber);
    delay(6000);  
}

void nfcHandler(){
  Serial.print("Card read\n");
  nfcRead(&ci);
}

void getAdafruitSubscription(systemStatus status){
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
          break;
        case CHICKEN:
         Serial.printf("Cooking Chicken\n");
          break;
        case MACCHEESE:
         Serial.printf("Cooking Mac & Cheese\n");
          break;
        case STEAK:
        Serial.printf("Cooking Steak\n");
           break;
        case TURKEY:
        Serial.printf("Cooking Turkey\n");
           break;
      }
    }
  }
}
  
