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

// Define static (private) member
FlashQueue<int> FlashQueueInstance(
    BytesPerFlashPage,
    LowestFlashPageAvailable,
    HighestFlashPageAvailable);

test(noDataFlashRead)
{
  assertFalse(flashTranslationLayer->flashPopData());
}

test(fullPageWriteRead) 
{
  assertFalse(flashTranslationLayer->flashPopData());
  
  DataPacket dataPackets[flashTranslationLayer->numDataPacketsPerFlashPage()];
  for (int i = 0; i < flashTranslationLayer->numDataPacketsPerFlashPage(); i++)
  {
    dataPackets[i] = DataPacket(BatteryUpdateDatum { .SecondsSince1970Int = i, .SecondsSincePowerOn = i, .BatteryVoltage = i, ._unused0 = i });
  }

  assertTrue(flashTranslationLayer->flashWriteData(dataPackets));

  for (int i = 0; i < flashTranslationLayer->numDataPacketsPerFlashPage(); i++)
  {
    // Arduino Unit only supports simple type assertions - this is sufficient
    assertEqual(dataPackets[i].Payload.BatteryUpdate.SecondsSincePowerOn, flashTranslationLayer->flashPeekData().Payload.BatteryUpdate.SecondsSincePowerOn);
    assertTrue(flashTranslationLayer->flashPopData());
  }

  assertFalse(flashTranslationLayer->flashPopData());
}

test(fullFlashMemoryWriteRead)
{
  assertFalse(flashTranslationLayer->flashPopData());

  DataPacket dataPackets[flashTranslationLayer->numDataPacketsPerFlashPage()];
  
  for (int n = 0; n < flashTranslationLayer->numDataPacketFlashPages(); n++)
  {
    for (int i = 0; i < flashTranslationLayer->numDataPacketsPerFlashPage(); i++)
    {
      
      dataPackets[i] = DataPacket(BatteryUpdateDatum { .SecondsSince1970Int = 0, .SecondsSincePowerOn = i + n * flashTranslationLayer->numDataPacketsPerFlashPage(), .BatteryVoltage = 0, ._unused0 = 0 });
    }
  
    assertTrue(flashTranslationLayer->flashWriteData(dataPackets));
  }

  // Now should fail given there are no flash pages left to write
  assertFalse(flashTranslationLayer->flashWriteData(dataPackets));

  for (int n = 0; n < flashTranslationLayer->numDataPacketFlashPages(); n++)
  {
    for (int i = 0; i < flashTranslationLayer->numDataPacketsPerFlashPage(); i++)
    {
      assertEqual(i + n * flashTranslationLayer->numDataPacketsPerFlashPage(), flashTranslationLayer->flashPeekData().Payload.BatteryUpdate.SecondsSincePowerOn);
      assertTrue(flashTranslationLayer->flashPopData());
    }
  }
  
  assertFalse(flashTranslationLayer->flashPopData());
}

void setup() {
  // enable logging to see output of text
  #ifndef DISABLE_LOGGING   
    // Initialize with log level and log output.
    Serial.begin(9600); 
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  #endif

  flashTranslationLayer = V2FlashTranslationLayer::getInstance();

  // sanity check
  assertTrue(flashTranslationLayer->numDataPacketFlashPages() > 0);
  assertTrue(flashTranslationLayer->numDataPacketsPerFlashPage() > 0);

  // Clear any flash memory in case leftover from crashed / previous execution
  for (int n = 0; n < flashTranslationLayer->numDataPacketFlashPages(); n++)
  {
    for (int i = 0; i < flashTranslationLayer->numDataPacketsPerFlashPage(); i++)
    {
      if (!flashTranslationLayer->flashPopData())
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
