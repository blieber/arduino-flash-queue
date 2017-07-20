#include <ArduinoUnit.h>  // Convenient library for running unit tests on an Arduino board
#include <ArduinoLog.h>  // Convenient logging library that wraps Arduino Serial
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

  int dataPackets[flashQueue.DataPacketsPerFlashPage];
  for (int i = 0; i < flashQueue.DataPacketsPerFlashPage; i++)
  {
    dataPackets[i] = i;
  }

  assertTrue(flashQueue.flashWriteData(dataPackets));

  for (int i = 0; i < flashQueue.DataPacketsPerFlashPage; i++)
  {
    // Arduino Unit only supports simple type assertions - this is sufficient
    assertEqual(dataPackets[i], flashQueue.flashPeekDatum());
    assertTrue(flashQueue.flashPopDatum());
  }

  assertFalse(flashQueue.flashPopDatum());
}

test(fullFlashMemoryWriteRead)
{
  assertFalse(flashQueue.flashPopDatum());

  int dataPackets[flashQueue.DataPacketsPerFlashPage];

  for (int n = 0; n < flashQueue.NumFlashPages; n++)
  {
    for (int i = 0; i < flashQueue.DataPacketsPerFlashPage; i++)
    { 
      dataPackets[i] = n * flashQueue.DataPacketsPerFlashPage + i;
    }

    assertTrue(flashQueue.flashWriteData(dataPackets));
  }

  // Now should fail given there are no flash pages left to write
  assertFalse(flashQueue.flashWriteData(dataPackets));

  for (int n = 0; n < flashQueue.NumFlashPages; n++)
  {
    for (int i = 0; i < flashQueue.DataPacketsPerFlashPage; i++)
    {
      assertEqual(i + n * flashQueue.DataPacketsPerFlashPage, flashQueue.flashPeekDatum());
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

  // sanity check
  assertTrue(flashQueue.NumFlashPages > 0);
  assertTrue(flashQueue.DataPacketsPerFlashPage > 0);

  // Clear any flash memory in case leftover from crashed / previous execution
  for (int n = 0; n < flashQueue.NumFlashPages; n++)
  {
    for (int i = 0; i < flashQueue.DataPacketsPerFlashPage; i++)
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
