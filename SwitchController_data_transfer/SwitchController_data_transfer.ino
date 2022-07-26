#include "LUFAConfig.h"
#include <LUFA.h>
#include "joystick.h"
#include <SPI.h>
#include <SD.h>

File myFile;
#define CS A0
#define DATASPEED 3 // this is the fastes that the switch can detect

int frameNumber = 0;
int joyData = 0;
int tick=0;
bool demoTest = false;

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

/*
The following ButtonMap variable defines all possible buttons within the
original 13 bits of space, along with attempting to investigate the remaining
3 bits that are 'unused'. This is what led to finding that the 'Capture'
button was operational on the stick.
*/
uint16_t ButtonMap[16] = {
  0x01,
  0x02,
  0x04,
  0x08,
  0x10,
  0x20,
  0x40,
  0x80,
  0x100,
  0x200,
  0x400,
  0x800,
  0x1000,
  0x2000,
  0x4000,
  0x8000,
};


// Main entry point.
int main(void) {
  // We'll start by performing hardware and peripheral setup.
  SetupHardware();
  // We'll then enable global interrupts for our use.
  GlobalInterruptEnable();

  pinMode(A1, INPUT_PULLUP); // for L + R
  pinMode(2, INPUT_PULLUP); // A
  SD.begin(CS);

    myFile = SD.open("bitmap.bmp"); // open the file for reading
  
  // Once that's done, we'll enter an infinite loop.
  for (;;)
  {
    // We need to run our task to process and deliver data for our IN and OUT endpoints.
    HID_Task();
    // We also need to run the main USB management task.
    USB_USBTask();
  }
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
  // We need to disable watchdog if enabled by bootloader/fuses.
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  // We need to disable clock division before initializing the USB hardware.
  clock_prescale_set(clock_div_1);
  // We can then initialize our hardware and peripherals, including the USB stack.
  USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
  // We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
  // We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
  bool ConfigSuccess = true;

  // We setup the HID report endpoints.
  ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
  ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

  // We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
  // We can handle two control requests: a GetReport and a SetReport.
  switch (USB_ControlRequest.bRequest)
  {
    // GetReport is a request for data from the device.
    case HID_REQ_GetReport:
      if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
      {
        // We'll create an empty report.
        USB_JoystickReport_Input_t JoystickInputData;
        // We'll then populate this report with what we want to send to the host.
        GetNextReport(&JoystickInputData);
        // Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
        Endpoint_ClearSETUP();
        // Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
        Endpoint_Write_Control_Stream_LE(&JoystickInputData, sizeof(JoystickInputData));
        // We then acknowledge an OUT packet on this endpoint.
        Endpoint_ClearOUT();
      }

      break;
    case HID_REQ_SetReport:
      if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
      {
        // We'll create a place to store our data received from the host.
        USB_JoystickReport_Output_t JoystickOutputData;
        // Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
        Endpoint_ClearSETUP();
        // With our report available, we read data from the control stream.
        Endpoint_Read_Control_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData));
        // We then send an IN packet on this endpoint.
        Endpoint_ClearIN();
      }

      break;
  }
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
  // If the device isn't connected and properly configured, we can't do anything here.
  if (USB_DeviceState != DEVICE_STATE_Configured)
    return;

  // We'll start with the OUT endpoint.
  Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
  // We'll check to see if we received something on the OUT endpoint.
  if (Endpoint_IsOUTReceived())
  {
    // If we did, and the packet has data, we'll react to it.
    if (Endpoint_IsReadWriteAllowed())
    {
      // We'll create a place to store our data received from the host.
      USB_JoystickReport_Output_t JoystickOutputData;
      // We'll then take in that data, setting it up in our storage.
      Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL);
      // At this point, we can react to this data.
      // However, since we're not doing anything with this data, we abandon it.
    }
    // Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
    Endpoint_ClearOUT();
  }

  // We'll then move on to the IN endpoint.
  Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
  // We first check to see if the host is ready to accept data.
  if (Endpoint_IsINReady())
  {
    // We'll create an empty report.
    USB_JoystickReport_Input_t JoystickInputData;
    // We'll then populate this report with what we want to send to the host.
    GetNextReport(&JoystickInputData);
    // Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
    Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL);
    // We then send an IN packet on this endpoint.
    Endpoint_ClearIN();

    /* Clear the report data afterwards */
    // memset(&JoystickInputData, 0, sizeof(JoystickInputData));
  }
}

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {

  /* Clear the report contents */
  memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));

  // L + R are needed to connect the controller
  if(digitalRead(A1)==LOW){
    ReportData->Button |= (0x010 | 0x020); // L + R
    demoTest=false;
    frameNumber=0;
    tick=0;
    myFile.seek(0);
  }

  // A will be used to start
  if(digitalRead(2)==LOW){
    while(digitalRead(2)==LOW){
      ;;
    }
    demoTest = true;
    joyData=0;
    frameNumber=0;
  }

  if(demoTest){
    if(frameNumber==0){
      if (tick==0){
        if((joyData = myFile.read())==-1){
          demoTest=false;
          myFile.close();
        }
      }
      tick = !tick;
    }
    if(++frameNumber == DATASPEED){
      frameNumber=0;
    }

    
    ReportData->Button = joyData;
    ReportData->Button |= 1<<11; // button to show sending data kind of CS
    ReportData->Button |= (tick&1)<<10;
  }


  ReportData->HAT = 0x08; // centre the hat
  ReportData->LX = 128;
  ReportData->LY = 128;
  ReportData->RX = 128;
  ReportData->RY = 128;
  
}
