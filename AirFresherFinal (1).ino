//                                       LIBRARIES

#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NewPing.h>

//----------------------------------------------------------------------------------------------
//                                    PIN DECLARATIONS
const byte menuButtonsPin     =   A0;  
const byte sprayButtonPin     =   3;
const byte contactSensorPin   =   A1;
const byte lightSensorPin     =   A2;
const byte motionSensorPin    =   2;
const byte LEDsPin            =   10;
const byte LEDPin             =   1;
const byte fresherPin         =   13;
#define ONE_WIRE_BUS              6
#define TRIGGER_PIN               8  // Arduino pin tied to trigger pin on ping sensor.
#define ECHO_PIN                  7  // Arduino pin tied to echo pin on ping sensor.
LiquidCrystal lcd(12, 11, 5, 4, 9, 0);

//----------------------------------------------------------------------------------------------
//                                      SENSOR SETUP

//Temperature sensor setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//Distance sensor setup
#define MAX_DISTANCE 300 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
int dist;
unsigned int pingSpeed = 100; // How frequently are we going to send out a ping (in milliseconds). 50ms would be 20 times a second.
unsigned long pingTimer;     // Holds the next ping time.

//----------------------------------------------------------------------------------------------
//                                      TIMERS & DELAYS

//Timers
unsigned long sprayTimer;
unsigned long messageTimer;
unsigned long toiletTimer;
unsigned long temperatureTimer;

//Timerstates for small and big message
byte switchTimer = 0;
byte switchSensor = 0; //0 = niets meer detecteren, 1 is wel iets detecteren

//Delays
long freshnerDelay = 60000;     //1 minute default
long temperatureDelay = 2000;   //Each two seconds the temperature is updated

//----------------------------------------------------------------------------------------------
//                                        OTHER VARIABLES

int timesToSpray = 0;
volatile int spraysLeft = 2400;
enum normalStates {TOU_NIU, TOU_U, TOU_C, TOU_1, TOU_2, SPRAY, MENU};
int state = TOU_NIU;
long temperature;
int contact;
int light;
int distance;


//----------------------------------------------------------------------------------------------
//                                        SETUP & LOOP

void setup() {
  //Serial.begin(9600);

  //Set pin modes
  pinMode(menuButtonsPin, INPUT);
  pinMode(sprayButtonPin, INPUT);
  pinMode(contactSensorPin, INPUT);
  pinMode(lightSensorPin, INPUT);
  pinMode(motionSensorPin, INPUT);
  pinMode(LEDPin, OUTPUT);
  pinMode(fresherPin, OUTPUT);

  //Attach interrupts
  attachInterrupt(digitalPinToInterrupt(sprayButtonPin), interruptSprayButtonSensor, RISING); //Pullup resistor is used
  attachInterrupt(digitalPinToInterrupt(motionSensorPin), interruptMotionSensor, RISING);

  //Start up library for temperature
  sensors.begin();

  //Set timers
  sprayTimer = millis();
  messageTimer = millis();
  toiletTimer = millis();
  pingTimer = millis();   //Start timer for distance sensor
  temperatureTimer = millis();

  //LCD setup
  lcd.begin(16, 2);
}

void loop() {
  
  if(state != MENU){
    stateMachine();
    temperatureSensorInput();
  }
  else{
    menuStateMachine();
  }

  menuButtonsSensor();
}

//----------------------------------------------------------------------------------------------
//                                        MENU BUTTONS

//Variables for button states
int currentScrollButtonState;
int scrollButtonState;
int lastScrollButtonState = LOW;
int currentClickButtonState;
int clickButtonState;
int lastClickButtonState = LOW;

//Variables for menu button debouncing;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 25;

//Detects whether one of the menu buttons is pressed
void menuButtonsSensor(){
  //Reads input of button pin
  int analogVolt = analogRead(menuButtonsPin);

  //Serial.println(analogVolt);

  //Test which button is pressed
  if(analogVolt > 1000){
    currentScrollButtonState = LOW;
    currentClickButtonState = LOW;
  }
  else if(analogVolt < 100){
    currentScrollButtonState = HIGH;
  }
  else if(analogVolt > 500 && analogVolt < 600){
    currentClickButtonState = HIGH;
  }
  
  //Scroll button debounce
  if(currentScrollButtonState != lastScrollButtonState){
    lastDebounceTime = millis();  
  }

  if((millis() - lastDebounceTime) > debounceDelay){
    if(currentScrollButtonState != scrollButtonState){
      scrollButtonState = currentScrollButtonState;

      if(scrollButtonState == HIGH){
        Serial.println("SCROLL");
        scrollNavigation();
      }
    }  
  }
  
  lastScrollButtonState = currentScrollButtonState;
  
  //Click button debounce
  if(currentClickButtonState != lastClickButtonState){
    lastDebounceTime = millis();  
  }

  if((millis() - lastDebounceTime) > debounceDelay){
    if(currentClickButtonState != clickButtonState){
      clickButtonState = currentClickButtonState;

      if(clickButtonState == HIGH){
        Serial.println("CLICK");
        clickNavigation();
      }
    }  
  }

  lastClickButtonState = currentClickButtonState;
}

//----------------------------------------------------------------------------------------------
//                                        SPRAY BUTTON

//Detects whether spray button is pressed and acts as an interrupt
void interruptSprayButtonSensor(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200)
  {
    //Serial.println("TEST");
    timesToSpray = 1;
    state = SPRAY;
  }
   last_interrupt_time = interrupt_time;
}

//----------------------------------------------------------------------------------------------
//                                        CONTACT SENSOR

//Detects contact sensor input
int contactSensorInput(){
  int analogContactState = analogRead(contactSensorPin);
  
  int sensorState;
  if(analogContactState < 300)
    sensorState = HIGH;
  else
    sensorState = LOW;
  
  return sensorState;
}

//----------------------------------------------------------------------------------------------
//                                        LIGHT SENSOR

//Detects light sensor input
int lightSensorInput(){
  return analogRead(lightSensorPin);
}

//----------------------------------------------------------------------------------------------
//                                        MOTION SENSOR

//Detects whether the motions sensor is activated and acts as an interrupt
void interruptMotionSensor(){
  Serial.println("Er is motion!");
  state = TOU_C;
}

//----------------------------------------------------------------------------------------------
//                                     TEMPERATURE SENSOR

//Detects temperature sensor input
void temperatureSensorInput(){

    if(millis() - temperatureTimer > temperatureDelay){
      sensors.requestTemperatures();
      temperature = sensors.getTempCByIndex(0);
      temperatureTimer = millis();  
    }
   
}

//----------------------------------------------------------------------------------------------
//                                      DISTANCE SENSOR

//Detects distance sensor input
int distanceSensorInput(){
  distanceSensor();

  return dist;
}

void distanceSensor(){
    // Notice how there's no delays in this sketch to allow you to do other processing in-line while doing distance pings.
  if (millis() >= pingTimer) {   // pingSpeed milliseconds since last ping, do another ping.
    pingTimer += pingSpeed;      // Set the next ping time.
    sonar.ping_timer(echoCheck); // Send out the ping, calls "echoCheck" function every 24uS where you can check the ping status.
  }
}

void echoCheck() { // Timer2 interrupt calls this function every 24uS where you can check the ping status.
  if (sonar.check_timer()) { // This is how you check to see if the ping was received.
    dist = sonar.ping_result / US_ROUNDTRIP_CM;
  }
}

//----------------------------------------------------------------------------------------------
//                                    STATE MACHINE

int lightDetection = 400;
int distanceDetection = 40;

void stateMachine(){
  switch (state){
    case TOU_NIU:  
      //Active sensors
      light = lightSensorInput();
      contact = contactSensorInput();

      //LEDs
      LEDsController(LOW, LOW, LOW);

      idleMenu();
       
      //If the lights go on and the door is open -> TOU_U
      
      if(light >= lightDetection && contact == LOW){
        state = TOU_U;
      }
      
      break;
    case TOU_U:
      //Active sensors
      contact = contactSensorInput();
      light = lightSensorInput();
      distance = distanceSensorInput();

      //LEDs
      LEDSearching();
       
      //If the door is closed, the lights are on and someone is on the toilet -> TOU_1
      if(contact == HIGH && light >= lightDetection && distance < distanceDetection){
        state = TOU_1;
        messageTimer = millis();
        //switchTimer = 0;
        //switchSensor = 0;
      }
      else if(contact == HIGH && light < lightDetection){   //Als de deur dicht gaat en het licht uit gaat) -> TOU_NIU
        state = TOU_NIU;
      }
      

      break;
    case TOU_C:
      //Active sensors
      contact = contactSensorInput();
      light = lightSensorInput();

      //LEDs
      LEDBlink();

      idleMenu();    

      //If the door is closed and there is no light  -> TOU_NIU   
      if(contact == HIGH && light < lightDetection){
        state = TOU_NIU;
      }

      break;
    case TOU_1:
      //Active sensors
      distance = distanceSensorInput();
      
      //LEDs
      LEDsController(HIGH, LOW, LOW);

      //Serial.println(distance);

      idleMenu();
    
      //If there is someone on the toilet after three minutes -> TOU_2
      //If there is noone on the toilet after three minutes -> SPRAY
      if(millis() - messageTimer > 180000){  //Three minutes  
        if(distance > distanceDetection){
          Serial.println("MESSAGE 1");
          timesToSpray = 1;
          state = SPRAY;
          sprayTimer = millis();
        }
        else{
          Serial.println("MESSAGE 2");
          timesToSpray = 2;
          state = TOU_2;  
          sprayTimer = millis();   
        }
      }
      
      break;
    case TOU_2:
      //Active sensors
      distance = distanceSensorInput();
      
      //LEDs 
      LEDsController(HIGH, HIGH, LOW);

      idleMenu();
      
      //If the distancesensor notices that noone is on the toilet (with an extra check) -> SPRAY*2
      toiletCheck();
      
      break;
    case SPRAY:
      //LEDs
      LEDsController(HIGH, HIGH, HIGH);

      idleMenu();
      
      //If spraying is over -> TOU_NIU   
      if(millis() - sprayTimer >= freshnerDelay){
        for(int i = timesToSpray; i > 0; i = i-1){
          Serial.println("SPRAY"); 
          spray(); 
          spraysLeft = spraysLeft - 1;
          delay(3000); 
        }
        
        timesToSpray = 0;
        state = TOU_NIU;
      } 
      
      break;
  }
}

//----------------------------------------------------------------------------------------------
//                                    CHECK TOILET

byte toiletIndex = 0;

void toiletCheck(){
  byte test1;
  byte test2;

  if(toiletIndex == 0){
    test1 = offToilet();
    toiletIndex = 1;
  }

  if(millis() - toiletTimer >= 5000){
    test2 = offToilet();

    if(test1 == HIGH && test2 == HIGH){
      state = SPRAY;
      sprayTimer = millis();
      Serial.println("He is off the toilet!");
    }
    else{
      Serial.println("He is not off the toilet...");
    }        
    toiletTimer = millis();
    toiletIndex = 0;
  }
}

byte offToilet(){
  byte result;
  
  if(distance < distanceDetection){
    result = LOW;
  }
  else{
    result = HIGH;
  }

  return result;
}

//----------------------------------------------------------------------------------------------
//                                   LED CONTROLLER

void LEDsController(byte LED1, byte LED2, byte LED3){
  if(LED1 == LOW && LED2 == LOW)  
    pinMode(LEDsPin, INPUT);      //LED 1 off, LED 2 off
  else if(LED1 == HIGH && LED2 == LOW){
    pinMode(LEDsPin, OUTPUT);
    digitalWrite(LEDsPin, HIGH);  //LED 1 on, LED 2 off    
  }
  else if(LED1 == LOW && LED2 == HIGH){
    pinMode(LEDsPin, OUTPUT);
    digitalWrite(LEDsPin, LOW);   //LED 1 off, LED 2 on
  } 
  else{
    pinMode(LEDsPin, OUTPUT);
    analogWrite(LEDsPin, 127);    //LED 1 on, LED 2 on
  }
     
  if(LED3 == LOW)                 
    digitalWrite(LEDPin, LOW);    //LED 3 off
  else                            
    digitalWrite(LEDPin, HIGH);   //LED 3 on
}

unsigned long blinkPreviousMillis;

void LEDBlink(){
  unsigned long currentMillis = millis();

  if((currentMillis - blinkPreviousMillis) > 500){
    LEDsController(HIGH, HIGH, HIGH);
  }
  
  if((currentMillis - blinkPreviousMillis) > 1000){
    blinkPreviousMillis = currentMillis;
    LEDsController(LOW, LOW, LOW);
  }
}

unsigned long searchPreviousMillis;

void LEDSearching(){
  unsigned long currentMillis = millis();
  
  if(currentMillis - searchPreviousMillis > 500)
    LEDsController(HIGH, LOW, LOW);
  
  if(currentMillis - searchPreviousMillis > 1000)
    LEDsController(HIGH, HIGH, LOW);

  if(currentMillis - searchPreviousMillis > 1500)
    LEDsController(HIGH, HIGH, HIGH);

  if(currentMillis - searchPreviousMillis > 2000){
    LEDsController(LOW, LOW, LOW);  
    searchPreviousMillis = millis();
  }

}

//----------------------------------------------------------------------------------------------
//                                   LCD CONTROLLER

void idleMenu(){
  lcd.setCursor(0,0);
  lcd.print("SPRAYS: " + String(spraysLeft));
  lcd.setCursor(0,1);
  lcd.print("TEMP: " + String(temperature));
}

//----------------------------------------------------------------------------------------------
//                                   MENU NAVIGATION

enum menuStates {MAIN, DELAY, RESET};
enum mainMenuStates {MAIN_DELAY, MAIN_RESET, MAIN_EXIT};
enum delayMenuStates {DELAY_1, DELAY_2, DELAY_5, DELAY_10, DELAY_15, DELAY_EXIT};
enum resetMenuStates {RESET_RESET, RESET_EXIT};

int menuState = MAIN;
int mainMenuState = MAIN_DELAY;
int delayMenuState = DELAY_1;
int resetMenuState = RESET_RESET;

void menuStateMachine(){
  //LEDs that need to be on
  LEDsController(LOW, LOW, HIGH);
  
  switch(state){
    case MENU:

      switch(menuState){
        case MAIN:
          lcd.setCursor(6,0);
          lcd.print("MENU");
          
          switch(mainMenuState){
            case MAIN_DELAY:
              blinkWord("DELAY", 0);
              printWord("RESET", 6);
              printWord("EXIT", 12);
              break;
            case MAIN_RESET:
              printWord("DELAY", 0);
              blinkWord("RESET", 6);
              printWord("EXIT", 12);
              break;
            case MAIN_EXIT:
              printWord("DELAY", 0);
              printWord("RESET", 6);
              blinkWord("EXIT", 12);
              break;
              
          }
          
          break;
        case DELAY:
          lcd.setCursor(4,0);
          int sprayDelay = freshnerDelay / 6000;
          lcd.print("DELAY: " + String(sprayDelay));


          switch(delayMenuState){
            case DELAY_1:
              blinkWord("1", 0);
              printWord("2", 2);
              printWord("5", 4);
              printWord("10", 6);
              printWord("15", 9);
              printWord("EXIT", 12);
              break;
            case DELAY_2:
              printWord("1", 0);
              blinkWord("2", 2);
              printWord("5", 4);
              printWord("10", 6);
              printWord("15", 9);
              printWord("EXIT", 12);
              break;
            case DELAY_5:
              printWord("1", 0);
              printWord("2", 2);
              blinkWord("5", 4);
              printWord("10", 6);
              printWord("15", 9);
              printWord("EXIT", 12);
              break;
            case DELAY_10:
              printWord("1", 0);
              printWord("2", 2);
              printWord("5", 4);
              blinkWord("10", 6);
              printWord("15", 9);
              printWord("EXIT", 12);
              break;
            case DELAY_15:
              printWord("1", 0);
              printWord("2", 2);
              printWord("5", 4);
              printWord("10", 6);
              blinkWord("15", 9);
              printWord("EXIT", 12);
              break; 
            case DELAY_EXIT:
              printWord("1", 0);
              printWord("2", 2);
              printWord("5", 4);
              printWord("10", 6);
              printWord("15", 9);
              blinkWord("EXIT", 12);
              break;  
                                               
          }

          break;
        case RESET:     
          Serial.println("I'm here");
          lcd.setCursor(2,0);
          lcd.print("SPRAYS: " + String(spraysLeft));

          switch(resetMenuState){
            case RESET_RESET:
              blinkWord("RESET", 0);
              printWord("EXIT", 12);
              break;
            case RESET_EXIT:
              printWord("RESET", 0);
              blinkWord("EXIT", 12);
              break;
          }
          
          break;
      }     
      break;

    case !MENU:
      state = MENU;
      menuState = MAIN;
      mainMenuState = MAIN_DELAY;
      break;
  }
}

void clickNavigation(){
  switch(state){
    case MENU:

      switch(menuState){
        case MAIN:

          switch(mainMenuState){
            case MAIN_DELAY:
              menuState = DELAY;
              delayMenuState = DELAY_1;
              lcd.clear();              
              break;
            case MAIN_RESET:
              menuState = RESET;
              resetMenuState = RESET_RESET;
              Serial.println("To RESET!");
              Serial.println(menuState);
              Serial.println(resetMenuState);
              lcd.clear();
              break;
            case MAIN_EXIT:
              state = TOU_NIU;
              lcd.clear();
              break;
              
          }
          
          break;
        case DELAY:

          switch(delayMenuState){
            case DELAY_1:
              freshnerDelay = 6000;
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;
            case DELAY_2:
              freshnerDelay = 12000;
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;
            case DELAY_5:
              freshnerDelay = 30000;
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;
            case DELAY_10:
              freshnerDelay = 60000;
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;
            case DELAY_15:
              freshnerDelay = 90000;
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break; 
            case DELAY_EXIT:
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;  
                                               
          }

          break;
        case RESET:

          switch(resetMenuState){
            case RESET_RESET:
              spraysLeft = 2400;
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;
            case RESET_EXIT:
              menuState = MAIN;
              mainMenuState = MAIN_DELAY;
              lcd.clear();
              break;
          }
          
          break;
      }     
      break;

    case !MENU:
      state = MENU;
      menuState = MAIN;
      mainMenuState = MAIN_DELAY;
      lcd.clear();
      break;
  }
}

void scrollNavigation(){
  switch(state){
    case MENU:

      switch(menuState){
        case MAIN:

          switch(mainMenuState){
            case MAIN_DELAY:
              mainMenuState = MAIN_RESET;
              break;
            case MAIN_RESET:
              mainMenuState = MAIN_EXIT;
              break;
            case MAIN_EXIT:
              mainMenuState = MAIN_DELAY;
              break;
              
          }
          
          break;
        case DELAY:

          switch(delayMenuState){
            case DELAY_1:
              delayMenuState = DELAY_2;
              break;
            case DELAY_2:
              delayMenuState = DELAY_5;
              break;
            case DELAY_5:
              delayMenuState = DELAY_10;
              break;
            case DELAY_10:
              delayMenuState = DELAY_15;
              break;
            case DELAY_15:
              delayMenuState = DELAY_EXIT;
              break; 
            case DELAY_EXIT:
              delayMenuState = DELAY_1;
              break;  
                                               
          }

          break;
        case RESET:

          switch(resetMenuState){
            case RESET_RESET:
              resetMenuState = RESET_EXIT;
              Serial.println("I'm HERE!");
              break;
            case RESET_EXIT:
              resetMenuState = RESET_RESET;
              break;
          }
          
          break;
      }     
      break;

    case !MENU:
      state = MENU;
      menuState = MAIN;
      mainMenuState = MAIN_DELAY;
      lcd.clear();
      break;
  }
}

int wordState = LOW;
int period_1 = 500;
unsigned long time_1 = 0;

void printWord(String text, int x){
  lcd.setCursor(x,1);
  lcd.print(text);
}

void blinkWord(String text, int x){
  String replacementText = createReplacementText(text);

  if(millis() > time_1 + period_1){
    time_1 = millis();
    if(wordState == HIGH){
      lcd.setCursor(x,1);
      lcd.print(replacementText);
      Serial.println(replacementText);
    }
    else if (wordState == LOW){
      lcd.setCursor(x,1);
      lcd.print(text);
      Serial.println(text);    
    }
    wordState = !wordState;
  }   
}

String createReplacementText(String text){
  String replacementText = "";
  int j = text.length();

  for(int i = 0; i < j; i++){
    replacementText += " ";
  }
  
  return replacementText;
}

//----------------------------------------------------------------------------------------------
//                                   SPRAY

void spray(){
  digitalWrite(fresherPin, HIGH);
  delay(1000);
  digitalWrite(fresherPin, LOW);
}
