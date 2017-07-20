# arduino-flash-queue
Facade with FIFO-ish interface to write to and read from flash memory on Arduino (Simblee) board


### Features
* High-level interface to write and read from flash memory instead of using manual page addressing, flashPageErase, and flashWriteBlock.
* Template class to allow custom types to be translated to/from flash memory.
* Order in which items are enqueued is preserved when items are dequeued (FIFO).
* (Not documented) Optional template parameter to save custom metadata with each flash page.


### Installation (Manual only supported at this point)
Download everything here and place top-level arduino-flash-queue folder in your [Arduino Libraries Directory](https://www.arduino.cc/en/Guide/Libraries) e.g. `<arduino installation directory>\libraries`.
C++11 compilation must be enabled (I believe this is the case for newer versions of Simblee Board SDK).


### Dependencies
* [ArduinoLog](https://github.com/thijse/Arduino-Log): C++ Log library for Arduino devices
* [ArduinoUnit](https://github.com/mmurdoch/arduinounit) (example/testing only): Unit test framework for arduino projects


### Usage

For a more detailed example see example.ino.

---

Pass the number of bytes per flash page, lowest available flash page, and highest available flash page to construct `FlashQueue<DataPacketType>`. FlashQueue then determines how many data packets can fit in a flash page based on the size of DataPacketType.

```
// Number of bytes per Simblee flash page
const int BytesPerFlashPage = 1024;

// Many examples in Simblee docs say up to 251 is available, but CAUTION - if use OTA 
// unfortunately this also uses some address space. According to 
// http://forum.rfduino.com/index.php?topic=1347.0 240-251 are used by the OTA bootloader. 
// Hence to be safe even in this example we only write up to 239.
const int HighestFlashPageAvailable = 239;

// Defined after linking - lowest page after all program memory.
const int LowestFlashPageAvailable = PAGE_FROM_ADDRESS(&_etextrelocate) + 1;

FlashQueue<DataPacketType> flashQueue(
    BytesPerFlashPage,
    LowestFlashPageAvailable,
    HighestFlashPageAvailable);
```

Only full-page writes are supported. Exactly flashQueue.DataPacketsPerFlashPage are assumed passed in as an argument and are written to the next available flash page.

```
DataPacketType dataPackets[flashQueue.DataPacketsPerFlashPage];

// Collect data packets to persist to flash ...

flashQueue.flashWriteData(dataPackets);
```

Reading follows a more familiar peek/pop pattern. A flash page is erased when the last packet located on the page is popped.
```
while (flashQueue.flashDataAvailable())
{
    DataPacketType dataPacket = flashQueue.flashPeekDatum();

    // Do something with dataPacket ...

    flashQueue.flashPopDatum();
}
```
