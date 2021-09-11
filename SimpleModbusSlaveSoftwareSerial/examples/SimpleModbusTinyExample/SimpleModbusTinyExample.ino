#include <SoftwareSerial.h>
#include <SimpleModbusSlaveSoftwareSerial.h>

#define  buttonPin0  2 // push button
#define  buttonPin1  3 // push button

/* This example code has 2 holding registers. 2 buttons,
   and 1 register to indicate errors encountered since started.

   The modbus_update() method updates the holdingRegs register array
   and checks communication.

   Note:
   The Arduino serial ring buffer is 128 bytes or 64 registers.
   Most of the time you will connect the arduino to a master via serial
   using a MAX485 or similar.

   In a function 3 request the master will attempt to read from your
   slave and since 5 bytes is already used for ID, FUNCTION, NO OF BYTES
   and two BYTES CRC the master can only request 122 bytes or 61 registers.

   In a function 16 request the master will attempt to write to your
   slave and since a 9 bytes is already used for ID, FUNCTION, ADDRESS,
   NO OF REGISTERS, NO OF BYTES and two BYTES CRC the master can only write
   118 bytes or 59 registers.

   Using the FTDI USB to Serial converter the maximum bytes you can send is limited
   to its internal buffer which is 60 bytes or 30 unsigned int registers.

   Thus:

   In a function 3 request the master will attempt to read from your
   slave and since 5 bytes is already used for ID, FUNCTION, NO OF BYTES
   and two BYTES CRC the master can only request 54 bytes or 27 registers.

   In a function 16 request the master will attempt to write to your
   slave and since a 9 bytes is already used for ID, FUNCTION, ADDRESS,
   NO OF REGISTERS, NO OF BYTES and two BYTES CRC the master can only write
   50 bytes or 25 registers.

   Since it is assumed that you will mostly use the Arduino to connect to a
   master without using a USB to Serial converter the internal buffer is set
   the same as the Arduino Serial ring buffer which is 128 bytes.
*/


// Using the enum instruction allows for an easy method for adding and
// removing registers. Doing it this way saves you #defining the size
// of your slaves register array each time you want to add more registers
// and at a glimpse informs you of your slaves register layout.

//////////////// registers of your slave ///////////////////
enum {
    // just add or remove registers and your good to go...
    // The first register starts at address 0
    BUTTON0,
    BUTTON1,
    TOTAL_ERRORS,
    // leave this one
    TOTAL_REGS_SIZE
    // total number of registers for function 3 and 16 share the same register array
};

unsigned int holdingRegs[TOTAL_REGS_SIZE]; // function 3 and 16 register array
////////////////////////////////////////////////////////////

#define RX            0       // Arduino defined pin (PB0, package pin #5)
#define TX            1       // Arduino defined pin (PB1, package pin #6)
#define RS485_EN      2       // pin to set transmission mode on RS485 chip (PB2, package pin #7)
#define BAUD_RATE     115200  // baud rate for serial communication
#define deviceID      3       // this device address

SoftwareSerial mySerial(RX, TX);

void setup()
{
    /* parameters(
                  SoftwareSerial* comPort
                  long baudrate,
                  unsigned char ID,
                  unsigned char transmit enable pin,
                  unsigned int holding registers size)

       The transmit enable pin is used in half duplex communication to activate a MAX485 or similar
       to deactivate this mode use any value < 2 because 0 & 1 is reserved for Rx & Tx
    */

    modbus_configure(&mySerial, BAUD_RATE, deviceID, RS485_EN, TOTAL_REGS_SIZE);
    pinMode(buttonPin0, INPUT);
    pinMode(buttonPin1, INPUT);
}

void loop()
{
    // modbus_update() is the only method used in loop(). It returns the total error
    // count since the slave started. You don't have to use it but it's useful
    // for fault finding by the modbus master.
    holdingRegs[TOTAL_ERRORS] = modbus_update(holdingRegs);
    holdingRegs[BUTTON0] = analogRead(buttonPin0);
    delayMicroseconds(50);
    holdingRegs[BUTTON1] = analogRead(buttonPin1);
    delayMicroseconds(50);
}

