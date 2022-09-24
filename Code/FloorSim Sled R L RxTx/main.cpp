#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <AS5600.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// Ajust!!
#define Rail_R_or_L 0        // Right = 0 / Left = 1
#define Transmission_Rate 8 // Wait 10ms between sending
#define Debug 0              // Stop send just look at linear rail val over serial

#define WS28_LED_Pin 2
#define Contact_Pin 14
#define EncoderA_Pin 12
#define EncoderB_Pin 13

#define LED_Brightness 5 // 0-255
//#define Encoder_Intervall 5 // Read Freqency (5 = 200Hz)

#define USING_TIM_DIV1 false   // for shortest and most accurate timer
#define USING_TIM_DIV16 true   // for medium time and medium accurate timer
#define USING_TIM_DIV256 false // for longest timer but least accurate. Default

Adafruit_NeoPixel Pixel = Adafruit_NeoPixel(1, WS28_LED_Pin, NEO_GRB + NEO_KHZ800);

// ESP8266Timer RotaryEncoderTimer;

AMS_5600 ams5600;

// Mac adress of the Receiver
uint8_t broadcastAddress[] = {0x98, 0xCD, 0xAC, 0x24, 0x18, 0x40};

// Functions
void Update_LED();
void LinearEncoderISR();
void Read_RotaryEncoder();

// Variables
byte LEDstate = 0;
long Sled_Pos = 0;
long DeltaSledPos = 0;
int RotaryEncoderPos = 0;
int DeltaRotaryEncoderPos = 0;

unsigned int message_num = 0;
unsigned long lastTime = 0;

// Message Structure
typedef struct struct_message
{
  bool x = Rail_R_or_L; // Left or Right Rail
  int a;                // delta Move
  int b;                // delta Turn
  long c;               // Abs Move
  long d;               // Abs Turn
  int e;                //(Message num)
} struct_message;

// Create a struct_message called myData
struct_message myData;

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{
  if (sendStatus == 0)
  {
    // Change LED State to Green
    if (LEDstate == 0)
      LEDstate = 2; // Trurning LED Green

    // Set delta to 0 (if transimission was sucsessfull)
    DeltaRotaryEncoderPos = 0;
    DeltaSledPos = 0;
  }
  else
  {
    // Turning LED Orange
    LEDstate = 0;
    // Serial.println("Delivery fail"); // Turning LED Orange
  }
}

void setup()
{
  // Serial start
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  // I2C start
  Wire.begin();

  delay(300);

  // Pinmode
  pinMode(Contact_Pin, INPUT);
  pinMode(EncoderA_Pin, INPUT_PULLUP);
  pinMode(EncoderB_Pin, INPUT_PULLUP);

  Pixel.begin(); // Start indicator Pixel
  LEDstate = 1;
  Update_LED(); // Turning LED White

  attachInterrupt(digitalPinToInterrupt(EncoderA_Pin), LinearEncoderISR, RISING);
  delay(1500);
  Serial.println("Setup done");
  LEDstate = 2; // Turning LED green
}

void loop()
{
  if ((millis() - lastTime) > Transmission_Rate && !Debug)
  {
    Read_RotaryEncoder();
    Update_LED();
    // Set values to send
    myData.a = DeltaSledPos;          // delta Move
    myData.b = DeltaRotaryEncoderPos; // delta Turn
    myData.c = Sled_Pos;              // Abs Move
    myData.d = RotaryEncoderPos;      // Abs Turn
    myData.e = message_num;

    message_num++;
    // Send message via ESP-NOW
    esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
    lastTime = millis();
  }

  if (Debug)
  {
    Serial.print("Rail Pos =");
    Serial.println(Sled_Pos);
    delay(200);
  }
}

void Update_LED()
{

  switch (LEDstate)
  {
  case 0:
    Pixel.setPixelColor(0, Pixel.Color(LED_Brightness, LED_Brightness / 2, 0)); // Orange
    break;

  case 1:
    Pixel.setPixelColor(0, Pixel.Color(LED_Brightness, LED_Brightness, LED_Brightness)); // White
    break;

  case 2:
    Pixel.setPixelColor(0, Pixel.Color(0, constrain(LED_Brightness * 2, 0, 255), 0)); // Green
    break;

  case 3:
    Pixel.setPixelColor(0, Pixel.Color(0, LED_Brightness / 3, 0)); // Green
    break;

  default:
    Pixel.setPixelColor(0, Pixel.Color(0, 0, 0));
    break;
  }

  // Pixel.setPixelColor(0, Pixel.Color(0,5,0)); // To test (Red,Green,Blue)

  Pixel.show();
}

void IRAM_ATTR LinearEncoderISR()
{
  /* (Version with debounce)
  static unsigned long prev_micros = micros();

  if (micros() - prev_micros > 30)
  {
    prev_micros = micros();

    if (digitalRead(EncoderB_Pin)) // Vergleich hoch runterzählen encoder
    {
      Sled_Pos++;
      if (!digitalRead(Contact_Pin))
        DeltaSledPos++;
    }
    else
    {
      Sled_Pos--;
      if (!digitalRead(Contact_Pin))
        DeltaSledPos--;
    }
  }
  */

  if (digitalRead(EncoderB_Pin)) // Vergleich hoch runterzählen encoder
  {
    Sled_Pos++;
    if (!digitalRead(Contact_Pin))
      DeltaSledPos++;
  }
  else
  {
    Sled_Pos--;
    if (!digitalRead(Contact_Pin))
      DeltaSledPos--;
  }
}

void Read_RotaryEncoder()
{

  static int lastAngle = ams5600.getRawAngle();
  static int rotations = 0;
  int current_angle = ams5600.getRawAngle();
  if (lastAngle - current_angle > 2048)
    rotations++;
  if (lastAngle - current_angle < -2048)
    rotations--;
  lastAngle = current_angle;
  RotaryEncoderPos = current_angle + rotations * 4096;
  static int LastRotaryEncoderPos = RotaryEncoderPos;
  if (!digitalRead(Contact_Pin))
  {
    DeltaRotaryEncoderPos = DeltaRotaryEncoderPos + LastRotaryEncoderPos - RotaryEncoderPos;
    LastRotaryEncoderPos = RotaryEncoderPos;
    if (LEDstate == 3)
      LEDstate = 2;
  }
  else
  {
    LastRotaryEncoderPos = RotaryEncoderPos;
    if (LEDstate == 2)
      LEDstate = 3;
  }
}