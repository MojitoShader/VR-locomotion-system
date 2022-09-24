#include <Arduino.h>

#include "Adafruit_TinyUSB.h"


//Var
int X_Axis = 0;
int Y_Axis = 0;


//Funktionen

int parseData(String data, char MarKer) {   //Parser
  data.remove(data.indexOf(MarKer), 1);
  return data.toInt();
}



uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_MOUSE()
};

// USB HID object
Adafruit_USBD_HID usb_hid;


void setup()
{

  usb_hid.setPollInterval(1);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));

  usb_hid.begin();

  Serial.begin(115200);
  Serial1.begin(1000000);
  Serial1.setTimeout(1);

  // wait until device mounted
  while ( !USBDevice.mounted() ) delay(1);
  Serial.println("Adafruit TinyUSB HID Mouse example");
}



void loop()
{
  if (Serial1.available() > 0)
  {
    String In_String = "";
    In_String = Serial1.readString();


    if (In_String[0] == 'X')
    {
      X_Axis = parseData(In_String, 'X');
      //Serial.println("X_Axis"+String(X_Axis));
    }

    if (In_String[0] == 'Y')
    {
      Y_Axis = parseData(In_String, 'Y');
      //Serial.println("Y_Axis"+String(Y_Axis));
    }

  }


  if ( usb_hid.ready() )
  {

    usb_hid.mouseMove(0, X_Axis, Y_Axis); // no ID: right + down
    Y_Axis = 0;
    X_Axis = 0;
  }

  
}