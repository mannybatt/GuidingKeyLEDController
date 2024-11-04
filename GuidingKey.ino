



/**
 * 
 *  Designed to be the ultimate RGB controller, this code listens for MQTT commands
 *  that change the states of relay sending RGB data to a string of addressable 
 *  lights. It switches between PixelBlaze, WLED, and a custom Twinkle Pattern. 
 *    -Manny Batt
 * 
 */


// ***************************************
// ********** Global Variables ***********
// ***************************************


//Globals for Wifi Setup and OTA
#include <credentials.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//WiFi Credentials
#ifndef STASSID
#define STASSID "your_ssid"
#endif
#ifndef STAPSK
#define STAPSK  "your_password"
#endif
const char* ssid = STASSID;
const char* password = STAPSK;

//MQTT
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#ifndef AIO_SERVER
#define AIO_SERVER      "your_MQTT_server_address"
#endif
#ifndef AIO_SERVERPORT
#define AIO_SERVERPORT  0000 //Your MQTT port
#endif
#ifndef AIO_USERNAME
#define AIO_USERNAME    "your_MQTT_username"
#endif
#ifndef AIO_KEY
#define AIO_KEY         "your_MQTT_key"
#endif
#define MQTT_KEEP_ALIVE 150
unsigned long previousTime;

//Initialize and Subscribe to MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe guidingKeySub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/GuidingKey_Status");
Adafruit_MQTT_Publish guidingKeyPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/GuidingKey_Status");

//Initialize Relays
#define r_preData D1
#define r_data D0
#define r_ground D5
#define r_vcc D6

//RGB
#include <Adafruit_NeoPixel.h>
#define PIN D4
#define Pixels 103
Adafruit_NeoPixel strip = Adafruit_NeoPixel(Pixels, PIN, NEO_GRB + NEO_KHZ800);
float redStates[Pixels];
float blueStates[Pixels];
float greenStates[Pixels];
float fadeRate = 0.96;

//System
uint16_t state = 0; //0=pixelBlaze 1=WLed 2=twinkleStars
uint16_t value = 0;
uint16_t twinkleConstant = 20;




// ***************************************
// *************** Setup *****************
// ***************************************


void setup() {

  delay(100);
  pinMode(r_preData, OUTPUT);
  pinMode(r_data, OUTPUT);
  pinMode(r_ground, OUTPUT);
  pinMode(r_vcc, OUTPUT);
  delay(100);                   //LOW = Not Activated
  digitalWrite(r_preData, LOW); //LOW = TwinkleStars   HIGH = WLed
  digitalWrite(r_data, LOW);    //LOW = preData   HIGH = PixelBlaze
  digitalWrite(r_ground, LOW);
  digitalWrite(r_vcc, LOW);

  strip.begin();
  strip.show();
  
  for(uint16_t l = 0; l < Pixels; l++) {
    redStates[l] = 0;
    greenStates[l] = 0;
    blueStates[l] = 0;
  }

  //Initialize Serial
  Serial.begin(115200);
  Serial.println("Booting");

  //Initialize Stars
  digitalWrite(r_vcc, HIGH);
  toWLed();

  //Initialize WiFi & OTA
  wifiSetup();

  //Initialize MQTT
  mqtt.subscribe(&guidingKeySub);

  Serial.println("***** May your heart be your guiding key *****");
}




// ***************************************
// ************* Da Loop *****************
// ***************************************


void loop() {

  //OTA & MQTT
  ArduinoOTA.handle();
  MQTT_connect();

  //Listen for Changes
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(0.01 /*1000*/))) {
    uint16_t value = atoi((char *)guidingKeySub.lastread);
    state = value;
    delay(250);
  }

  //Implement States
  // [PixelBlaze]
  if (state == 0) {
    toPixelBlaze();
  }
  // [WLed]
  else if (state == 1) {
    toWLed();
  }
  // [TwinkleStars]
  else if (state == 2) {
    toTwinkleStars();
  }
  // [Lower TwinkleConstant]
  else if (state == 3) {
    if(twinkleConstant > -1){
      twinkleConstant--;
    }
  }
  // [Raise TwinkleConstant]
  else if (state == 4) {
    if(twinkleConstant < 41){
      twinkleConstant++;
    }
  }
  else{
    if (random(twinkleConstant) == 1) {
      int i = random(Pixels);
      if (redStates[i] < 1 && greenStates[i] < 1 && blueStates[i] < 1) {
        redStates[i] = random(256);
        greenStates[i] = random(256);
        blueStates[i] = random(256);
      }
    }
    
    for(int l = 0; l < Pixels; l++) {
      if (redStates[l] > 1 || greenStates[l] > 1 || blueStates[l] > 1) {
        strip.setPixelColor(l, redStates[l], greenStates[l], blueStates[l]);
        
        if (redStates[l] > 1) {
          redStates[l] = redStates[l] * fadeRate;
        } else {
          redStates[l] = 0;
        }
        
        if (greenStates[l] > 1) {
          greenStates[l] = greenStates[l] * fadeRate;
        } else {
          greenStates[l] = 0;
        }
        
        if (blueStates[l] > 1) {
          blueStates[l] = blueStates[l] * fadeRate;
        } else {
          blueStates[l] = 0;
        }
        
      } else {
        strip.setPixelColor(l, 0,0,0);
      }
    }
    strip.show();
    delay(1); //Change this value to determine the speed of the pattern  
  }
}




// ***************************************
// ********** Backbone Methods ***********
// ***************************************


void toPixelBlaze() {
  Serial.println("toPixelBlaze");
  state = -1;
  ledReset();
  delay(500);
  digitalWrite(r_preData, LOW); //LOW = TwinkleStars   HIGH = WLed
  digitalWrite(r_data, HIGH);    //LOW = preData   HIGH = PixelBlaze
  digitalWrite(r_ground, HIGH);
  digitalWrite(r_vcc, HIGH);
}

void toWLed() {
  Serial.println("toWLed");
  state = -1;
  ledReset();
  delay(500);
  digitalWrite(r_preData, HIGH); //LOW = TwinkleStars   HIGH = WLed
  digitalWrite(r_data, LOW);    //LOW = preData   HIGH = PixelBlaze
  digitalWrite(r_ground, HIGH);
  digitalWrite(r_vcc, HIGH);
}

void toTwinkleStars() {
  Serial.println("toTwinkleStars");
  state = -1;
  ledReset();
  delay(500);
  digitalWrite(r_preData, LOW); //LOW = TwinkleStars   HIGH = WLed
  digitalWrite(r_data, LOW);    //LOW = preData   HIGH = PixelBlaze
  digitalWrite(r_ground, HIGH);
  digitalWrite(r_vcc, HIGH);
}

void ledReset() {
  digitalWrite(r_ground, LOW);
  digitalWrite(r_vcc, LOW);
}

void wifiSetup() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname("GuidingKey");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    //Serial.println("Connected");
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      //while (1);
      Serial.println("Wait 10 min to reconnect");
      delay(600000);
    }
  }
  Serial.println("MQTT Connected!");
}
