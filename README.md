# SimpleModbus Slave

SimpleModbus is a collection of Arduino libraries that enables you to communicate serially using the Modicon Modbus RTU protocol.

This project was born as an updated version of http://code.google.com/p/simple-modbus/ by Bester Juan. More important the code is now on github, so you can contribute more easily.

## Difference for original GitHub [repo](https://github.com/angeloc/simplemodbusng)

* Only slave module
* Single folder so it can be installed via `platformio lib`
* Automatically switches from SoftwareSerial(10, 11) on Uno to Serial3(15, 14) on Due

