// Arduino
#include <Arduino.h>

// ESP-Now (ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>

// Move Settings
#define Move_multiplier -1
#define Turn_multiplier 0.5

// Display
#include <Wire.h>
#include "SSD1306Wire.h" // legacy: #include "SSD1306.h"

// Display
SSD1306Wire display(0x3c, SDA, SCL);

// Smooth settings
#define Lengh_Smooth_Array 20

// Variables
byte R_connect = 0;
byte L_connect = 0;

int Y_Val = 0;

byte Mode = 1;

int direction = 0;
long R_Ang_Offset = 0;
long L_Ang_Offset = 0;

// Structure
typedef struct struct_message
{
  bool x; // Left or Right Rail
  int a;  // delta Move
  int b;  // delta Turn
  long c; // Abs Move
  long d; // Abs Turn
  int e;  //(Message num)
} struct_message;

// Create a struct_message called myData
struct_message myData;

// Var Right Sled
int R_deltaMove = 0;
int R_deltaTurn = 0;
long R_Pos = 0;
long R_Angle = 0;

// Var Right Sled
int L_deltaMove = 0;
int L_deltaTurn = 0;
long L_Pos = 0;
long L_Angle = 0;

// Functions
void Send_MouseInput();
void drawRunningBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress, uint8_t BarSize);
void sign(int val);                                                // Signum Function
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len); // Receive Function
void display_State();
void Error_check();

void setup()
{

  // Start Serial
  Serial.begin(115200);
  Serial1.begin(1000000);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  else
    Serial.println("Init ESP-NOW Sucsess");

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  // init Display
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  for (int i = 0; i < 100; i++) // Progress bar
  {
    display.clear();
    display.drawProgressBar(0, 32, 120, 10, i);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 15, "FloorSim by FlyingMarvin");
    display.display();
    delay(30);
  }
}

void loop()
{
  /*
  static byte Error_Timer = 0;
  if (Error_Timer >= 12)
  {
    Error_check();
    Error_Timer = 0;
  }
  else
    delay(1);
  Error_Timer++;
  Send_MouseInput();
  */
  Send_MouseInput();
  Error_check();
  delay(5);
}

void serialEvent() // Serial communication is used to change settings or set
{
  Serial.print("Got Input: ");
  char InputString = Serial.read();
  Serial.println(InputString);

  // Mode 1 (Turn By Turning foot while down)
  if (InputString == '1')
  {
    Mode = 1;
    Serial.println("Received Mode1");
  }

  // Mode 2 (Turn by holding Feet sideways and making walking Motion)
  if (InputString == '2')
  {
    Mode = 2;
    Serial.println("Received Mode2");
    // Measure Offsets
    L_Ang_Offset = L_Angle;
    R_Ang_Offset = R_Angle;
  }

  // Print Direction
  if (InputString == '3')
    Serial.println("Direction Angle: " + String(direction));
}

// Signum Function
static inline int8_t sgn(int val)
{
  if (val < 0)
    return -1;
  if (val == 0)
    return 0;
  return 1;
}

// Callback function that will be executed when data is received
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len)
{
  memcpy(&myData, incomingData, sizeof(myData));

  if (myData.x == 0) // Right Sled
  {
    R_deltaMove = myData.a + R_deltaMove;
    R_deltaTurn = myData.b + R_deltaTurn;
    R_Pos = myData.c;
    R_Angle = myData.d;
    R_connect = 0; // Reset connection supervisor   (Error counter)
  }
  if (myData.x == 1) // Left Sled
  {
    L_deltaMove = myData.a + L_deltaMove;
    L_deltaTurn = myData.b + L_deltaTurn;
    L_Pos = myData.c;
    L_Angle = myData.d;
    L_connect = 0; // Reset connection supervisor   (Error counter)
  }
}

void Send_MouseInput()
{
  // Calculate total delta Move
  int delta_move = abs(abs(R_deltaMove) - abs(L_deltaMove));
  if (abs(R_deltaMove) > abs(L_deltaMove))
    delta_move = delta_move * sgn(R_deltaMove);
  if (abs(L_deltaMove) > abs(R_deltaMove))
    delta_move = delta_move * sgn(L_deltaMove);

  delta_move = delta_move * Move_multiplier;

  /*

    //Movement Smooting
    static int Smothed_delta_move [Lengh_Smooth_Array];
    static int Smoth_array_counter = 0;
    Smoth_array_counter ++;
    if(Smoth_array_counter == Lengh_Smooth_Array)
      Smoth_array_counter = 0;

    Smothed_delta_move[Smoth_array_counter] = delta_move;

    static long sum = 0;
    sum = 0;
    for (int i = 0; i < Lengh_Smooth_Array; i++)
    {
      sum = sum + Smothed_delta_move[i];
    }

    delta_move = sum/Lengh_Smooth_Array;
  */

  int delta_turn = 0;

  if (Mode == 1) // Calculate total delta Turn (Mode 1)
  {
    delta_turn = abs(abs(R_deltaTurn) - abs(L_deltaTurn));
    if (abs(R_deltaTurn) > abs(L_deltaTurn))
      delta_turn = delta_turn * sgn(R_deltaTurn);
    if (abs(L_deltaTurn) > abs(R_deltaTurn))
      delta_turn = delta_turn * sgn(L_deltaTurn);
  }

  if (Mode == 2)
  {
    // calculation of the direction
    direction = ((L_Angle - L_Ang_Offset) + (R_Angle - R_Ang_Offset));

    delta_turn = direction * delta_move / 1000;
  }

  delta_turn = delta_turn * Turn_multiplier;

  // Send script
  static bool Switchbool = 0;

  if (Switchbool) // Swiches between the X and Y axis
  {
    Serial1.println("X" + String(delta_move)); // Sending val to mouse emulator microcontroller (Seeduino)
    Switchbool = false;
    R_deltaMove = 0; // Resets the Delta back to zero after sent
    L_deltaMove = 0;
  }
  else
  {
    Serial1.println("Y" + String(delta_turn));
    Switchbool = true;
    Y_Val = 0;
    L_deltaTurn = 0;
    R_deltaTurn = 0;
  }
}

void drawRunningBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress, uint8_t BarSize)
{
  uint16_t radius = height / 2;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t doubleRadius = 2 * radius;
  uint16_t innerRadius = radius - 2;

  display.setColor(WHITE);
  display.drawCircleQuads(xRadius, yRadius, radius, 0b00000110);
  display.drawHorizontalLine(xRadius, y, width - doubleRadius + 1);
  display.drawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
  display.drawCircleQuads(x + width - radius, yRadius, radius, 0b00001001);

  uint16_t ProgressBarPos = (width - doubleRadius + 1 - BarSize) * progress / 100;
  // Draw moving Bar
  display.fillCircle(xRadius + ProgressBarPos, yRadius, innerRadius);
  display.fillRect(xRadius + ProgressBarPos, y + 2, BarSize, height - 3);
  display.fillCircle(xRadius + ProgressBarPos + BarSize, yRadius, innerRadius);
}

void display_State()
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(30, 10, "System online");
  if (R_connect < 5)
  {
    display.drawString(0, 40, "R-rail connected");
  }
  else
  {
    display.drawString(0, 40, "R-rail fail");
  }

  if (L_connect < 5)
  {
    display.drawString(0, 50, "L-rail connected");
  }
  else
  {
    display.drawString(0, 50, "L-rail fail");
  }

  if (R_connect > 5 || L_connect > 5)
  {
    // Progress Bar / Active sign
    static byte Pos_RunningBar = 0;
    Pos_RunningBar++;
    if (Pos_RunningBar <= 100)
      drawRunningBar(0, 30, 120, 8, Pos_RunningBar, 30);
    if (Pos_RunningBar > 100)
      drawRunningBar(0, 30, 120, 8, 200 - Pos_RunningBar, 30);
    if (Pos_RunningBar == 200)
      Pos_RunningBar = 0;
  }

  
  display.display();
}

void Error_check()
{
  static bool ErrorErease = 1;
  if (R_connect > 5 || L_connect > 5)
  {
    display_State();
    ErrorErease = 1;
  }
  else if (ErrorErease)
  {
    display_State();
    ErrorErease = 0;
  }

  R_connect = constrain(R_connect + 1, 0, 10); // Connection supervisor (Gets resetet by OnDataRec) (Error counter)
  L_connect = constrain(L_connect + 1, 0, 10);
}