// Copyright (c) 2017 Quantac

#include <ArduinoUnit.h>
#include <ArduinoLog.h>
#include <arduino-flash-queue.hpp>

const int BytesPerFlashPage = 1024;

const int LowestFlashPageAvailable = PAGE_FROM_ADDRESS(&_etextrelocate) + 1;

// Many examples in Simblee docs say up to 251 is available, but CAUTION - if use OTA 
// unfortunately this also uses some of address space. According to 
// http://forum.rfduino.com/index.php?topic=1347.0 240-251 are used by the OTA bootloader. 
// Hence to be safe even in this example we only write up to 239.
const int HighestFlashPageAvailable = 239;

FlashQueue<int> flashQueue(
    BytesPerFlashPage,
    LowestFlashPageAvailable,
    HighestFlashPageAvailable);

test(noDataFlashRead)
{
  assertFalse(flashQueue.flashPopDatum());
}

test(fullPageWriteRead) 
{
  assertFalse(flashQueue.flashPopDatum());
  
  DataPacket dataPackets[flashQueue.numDataPacketsPerFlashPage()];
  for (int i = 0; i < flashQueue.numDataPacketsPerFlashPage(); i++)
  {
    dataPackets[i] = i;
  }

  assertTrue(flashQueue.flashWriteData(dataPackets));

  for (int i = 0; i < flashQueue.numDataPacketsPerFlashPage(); i++)
  {
    // Arduino Unit only supports simple type assertions - this is sufficient
    assertEqual(dataPackets[i].Payload.BatteryUpdate.SecondsSincePowerOn, flashQueue.flashPeekDatum().Payload.BatteryUpdate.SecondsSincePowerOn);
    assertTrue(flashQueue.flashPopDatum());
  }

  assertFalse(flashQueue.flashPopDatum());
}

test(fullFlashMemoryWriteRead)
{
  assertFalse(flashQueue.flashPopDatum());

  DataPacket dataPackets[flashQueue.numDataPacketsPerFlashPage()];
  
  for (int n = 0; n < flashQueue.numDataPacketFlashPages(); n++)
  {
    for (int i = 0; i < flashQueue.numDataPacketsPerFlashPage(); i++)
    {
      
      dataPackets[i] = DataPacket(BatteryUpdateDatum { .SecondsSince1970Int = 0, .SecondsSincePowerOn = i + n * flashQueue.numDataPacketsPerFlashPage(), .BatteryVoltage = 0, ._unused0 = 0 });
    }
  
    assertTrue(flashQueue.flashWriteData(dataPackets));
  }

  // Now should fail given there are no flash pages left to write
  assertFalse(flashQueue.flashWriteData(dataPackets));

  for (int n = 0; n < flashQueue.numDataPacketFlashPages(); n++)
  {
    for (int i = 0; i < flashQueue.numDataPacketsPerFlashPage(); i++)
    {
      assertEqual(i + n * flashQueue.numDataPacketsPerFlashPage(), flashQueue.flashPeekData().Payload.BatteryUpdate.SecondsSincePowerOn);
      assertTrue(flashQueue.flashPopDatum());
    }
  }
  
  assertFalse(flashQueue.flashPopDatum());
}

void setup() {
  // enable logging to see output of text
  #ifndef DISABLE_LOGGING   
    // Initialize with log level and log output.
    Serial.begin(9600); 
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  #endif

  flashQueue = V2flashQueue::getInstance();

  // sanity check
  assertTrue(flashQueue.numDataPacketFlashPages() > 0);
  assertTrue(flashQueue.numDataPacketsPerFlashPage() > 0);

  // Clear any flash memory in case leftover from crashed / previous execution
  for (int n = 0; n < flashQueue.numDataPacketFlashPages(); n++)
  {
    for (int i = 0; i < flashQueue.numDataPacketsPerFlashPage(); i++)
    {
      if (!flashQueue.flashPopDatum())
      {
        break;
      }
    }
  }
}

void loop()
{
  Test::run();
}
