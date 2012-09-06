# SimpleModbus NG

SimpleModbus is a collection of Arduino libraries that enables you to communicate serially using the Modicon Modbus RTU protocol.

This project was born as an updated version of http://code.google.com/p/simple-modbus/ by Bester Juan because it lacks support for commands other than 3 and 16. More important the code is on github, so you can contribute more easily.

## Features

This library adds support for command 6 and provides a more extensive support for arduino pins. The goal of the project is to support all usable MODBUS commands on arduino and to expose all arduino pins so you can use an arduino as an advanced automation controller for both analog/digital in/out.

## Usage
Simply copy the SimpleModbusMaster or SimpleModbusSlave or both into your Arduino IDE **libraries** folder. Than restart the ide and open the corresponding example into the example_master or example_slave folder.