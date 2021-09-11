#include <SimpleModbusMaster.h>

/* To communicate with a slave you need to create a
   packet that will contain all the information
   required to communicate to that slave.

   There are numerous counters for easy diagnostic.
   These are variables already implemented in a
   packet. You can set and clear these variables
   as needed.

   There are general modbus information counters:
   requests - contains the total requests to a slave
   successful_requests - contains the total successful requests
   total_errors - contains the total errors as a sum
   timeout - contains the total time out errors
   incorrect_id_returned - contains the total incorrect id returned errors
   incorrect_function_returned - contains the total incorrect function returned errors
   incorrect_bytes_returned - contains the total incorrect bytes returned errors
   checksum_failed - contains the total checksum failed errors
   buffer_errors - contains the total buffer errors

   And there are modbus specific exception counters:
   illegal_function - contains the total illegal function errors
   illegal_data_address - contains the total illegal data_address errors
   illegal_data_value - contains the total illegal data value errors
   misc_exceptions - contains the total miscellaneous returned exceptions

   And finally there is a variable called "connection" that
   at any given moment contains the current connection
   status of the packet. If true then the connection is
   active if false then communication will be stopped
   on this packet untill the programmer sets the "connection"
   variable to true explicitly. The reason for this is
   because of the time out involved in modbus communication.
   EACH faulty slave that's not communicating will slow down
   communication on the line with the time out value. E.g.
   Using a time out of 1500ms, if you have 10 slaves and 9 of them
   stops communicating the latency burden placed on communication
   will be 1500ms * 9 = 13,5 seconds!!!!

   modbus_update() returns the previously scanned false connection.
   You can use this as the index to your packet array to find out
   if the connection has failed in that packet and then react to it.
   You can then try to re-enable the connecion by setting the
   packet->connection attribute to true.
   The index will only be available for one loop cycle, after that
   it's cleared and ready to return the next false connection index
   if there is one else it will return the packet array size indicating
   everything is ok.

   All the error checking, updating and communication multitasking
   takes place in the background!

   In general to communicate with to a slave using modbus
   RTU you will request information using the specific
   slave id, the function request, the starting address
   and lastly the number of registers to request.
   Function 3 and 16 are supported. In addition to
   this broadcasting (id = 0) is supported on function 16.
   Constants are provided for:
   Function 3 -  READ_HOLDING_REGISTERS
   Function 16 - PRESET_MULTIPLE_REGISTERS

   The example sketch will read a packet consisting
   of 3 registers from address 0 using function 3 from
   the GM7U LS Industrial PLC (id = 2) and then write
   another packet containing the same 3 registers and
   the counter information of both packets using function 16
   to address 3 in the PLC. Using the supplied PLC software
   you can then view in realtime what the values are in
   the PLC's registers.
*/

// led to indicate that a communication error is present
#define connection_error_led 13

//////////////////// Port information ///////////////////
#define baud 9600
#define timeout 1000
#define polling 200 // the scan rate

// If the packets internal retry register matches
// the set retry count then communication is stopped
// on that packet. To re-enable the packet you must
// set the "connection" variable to true.
#define retry_count 10

// used to toggle the receive/transmit pin on the driver
#define TxEnablePin 2

// This is the easiest way to create new packets
// Add as many as you want. TOTAL_NO_OF_PACKETS
// is automatically updated.
enum {
    PACKET1,
    PACKET2,
    // leave this last entry
    TOTAL_NO_OF_PACKETS
};

// Create an array of Packets for modbus_update()
Packet packets[TOTAL_NO_OF_PACKETS];

// Create a packetPointer to access each packet
// individually. This is not required you can access
// the array explicitly. E.g. packets[PACKET1].id = 2;
// This does become tedious though...
packetPointer packet1 = &packets[PACKET1];
packetPointer packet2 = &packets[PACKET2];

// The data from the PLC will be stored
// in the regs array
unsigned int regs[9];

void setup()
{
    // read 3 registers starting at address 0
    packet1->id = 2;
    packet1->function = READ_HOLDING_REGISTERS;
    packet1->address = 0;
    packet1->no_of_registers = 3;
    packet1->register_array = regs;

    // write the 9 registers to the PLC starting at address 3
    packet2->id = 2;
    packet2->function = PRESET_MULTIPLE_REGISTERS;
    packet2->address = 3;
    packet2->no_of_registers = 9;
    packet2->register_array = regs;

    // P.S. the register_array entries above can be different arrays

    // Initialize communication settings etc...
    modbus_configure(baud, timeout, polling, retry_count, TxEnablePin, packets, TOTAL_NO_OF_PACKETS);

    pinMode(connection_error_led, OUTPUT);
}

void loop()
{
    unsigned int connection_status = modbus_update(packets);

    if (connection_status != TOTAL_NO_OF_PACKETS) {
        digitalWrite(connection_error_led, HIGH);
        // You could re-enable the connection by:
        //packets[connection_status].connection = true;
    } else
        digitalWrite(connection_error_led, LOW);

    // update the array with the counter data
    regs[3] = packet1->requests;
    regs[4] = packet1->successful_requests;
    regs[5] = packet1->total_errors;
    regs[6] = packet2->requests;
    regs[7] = packet2->successful_requests;
    regs[8] = packet2->total_errors;
}
