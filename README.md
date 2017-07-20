# arduino-flash-queue
Facade with FIFO-ish interface to write to and read from flash memory on Arduino (Simblee) board

### Installation (Manual only supported at this point)
Download everything here and place top-level arduino-flash-queue folder in your [Arduino Libraries Directory](https://www.arduino.cc/en/Guide/Libraries) e.g. `<arduino installation directory>\libraries`.
C++11 compilation must be enabled (I believe this is the case for newer versions of Simblee Board SDK).

### Dependencies
* [ArduinoLog](https://github.com/thijse/Arduino-Log): C++ Log library for Arduino devices
* [ArduinoUnit](https://github.com/mmurdoch/arduinounit) (example/testing only): Unit test framework for arduino projects