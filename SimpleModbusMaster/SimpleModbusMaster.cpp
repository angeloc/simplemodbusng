#include "SimpleModbusMaster.h"

#define BUFFER_SIZE 128

// modbus specific exceptions
#define ILLEGAL_FUNCTION 1
#define ILLEGAL_DATA_ADDRESS 2
#define ILLEGAL_DATA_VALUE 3

unsigned char transmission_ready_Flag;
unsigned char messageOkFlag, messageErrFlag;
unsigned char retry_count;
unsigned char TxEnablePin;
// frame[] is used to recieve and transmit packages. 
// The maximum number of bytes in a modbus packet is 256 bytes
// This is limited to the serial buffer of 128 bytes
unsigned char frame[BUFFER_SIZE]; 
unsigned int timeout, polling;
unsigned int T1_5; // inter character time out in microseconds
unsigned int T3_5; // frame delay in microseconds
unsigned long previousTimeout, previousPolling;
unsigned int total_no_of_packets;
Packet* packet; // current packet

// function definitions
void constructPacket();
void checkResponse();
void check_F3_data(unsigned char buffer);
void check_F16_data();
unsigned char getData();
void check_packet_status();
unsigned int calculateCRC(unsigned char bufferSize);
void sendPacket(unsigned char bufferSize);


unsigned int modbus_update(Packet* packets) 
{
	// Initialize the connection_status variable to the
	// total_no_of_packets. This value cannot be used as 
	// an index (and normally you won't). Returning this 
	// value to the main skecth informs the user that the 
	// previously scanned packet has no connection error.
	
	unsigned int connection_status = total_no_of_packets;

  if (transmission_ready_Flag) 
	{
	
		static unsigned int packet_index;	
	
		unsigned int failed_connections = 0;
	
		unsigned char current_connection;
	
		do
		{		
		
			if (packet_index == total_no_of_packets) // wrap around to the beginning
				packet_index = 0;
		
			// proceed to the next packet
			packet = &packets[packet_index];
		
			// get the current connection status
			current_connection = packet->connection;
		
			if (!current_connection)
			{
				connection_status = packet_index;
			
				// If all the connection attributes are false return
				// immediately to the main sketch
				if (++failed_connections == total_no_of_packets)
					return connection_status;
			}
		
			packet_index++;
			
		}while (!current_connection); // while a packet has no connection get the next one
		
		constructPacket();
	}
    
	checkResponse();
	
  check_packet_status();	
	
	return connection_status; 
}
  
void constructPacket()
{	 
	transmission_ready_Flag = 0; // disable the next transmission
	
  packet->requests++;
  frame[0] = packet->id;
  frame[1] = packet->function;
  frame[2] = packet->address >> 8; // address Hi
  frame[3] = packet->address & 0xFF; // address Lo
  frame[4] = packet->no_of_registers >> 8; // no_of_registers Hi
  frame[5] = packet->no_of_registers & 0xFF; // no_of_registers Lo
        
  unsigned int crc16;
	
  // construct the frame according to the modbus function  
  if (packet->function == PRESET_MULTIPLE_REGISTERS) 
  {
    unsigned char no_of_bytes = packet->no_of_registers * 2;
    unsigned char frameSize = 9 + no_of_bytes; // first 7 bytes of the array + 2 bytes CRC+ noOfBytes
    frame[6] = no_of_bytes; // number of bytes
    unsigned char index = 7; // user data starts at index 7
    unsigned int temp;
		unsigned char no_of_registers = packet->no_of_registers;
    for (unsigned char i = 0; i < no_of_registers; i++)
    {
      temp = packet->register_array[i]; // get the data
      frame[index] = temp >> 8;
      index++;
      frame[index] = temp & 0xFF;
      index++;
    }
    crc16 = calculateCRC(frameSize - 2);	
    frame[frameSize - 2] = crc16 >> 8; // split crc into 2 bytes
    frame[frameSize - 1] = crc16 & 0xFF;
    sendPacket(frameSize);
         
    if (packet->id == 0) // check broadcast id 
    {
			messageOkFlag = 1; // message successful, there will be no response on a broadcast
			previousPolling = millis(); // start the polling delay
		}
  }
	else // READ_HOLDING_REGISTERS is assumed
	{
		crc16 = calculateCRC(6); // the first 6 bytes of the frame is used in the CRC calculation
    frame[6] = crc16 >> 8; // crc Lo
    frame[7] = crc16 & 0xFF; // crc Hi
    sendPacket(8); // a request with function 3, 4 & 6 is always 8 bytes in size 
	}
}
  
void checkResponse()
{
	if (!messageOkFlag && !messageErrFlag) // check for response
  {
    unsigned char buffer = getData();
       
    if (buffer > 0) // if there's something in the buffer continue
    {
      if (frame[0] == packet->id) // check id returned
      {
				// to indicate an exception response a slave will 'OR' 
        // the requested function with 0x80 
				if ((frame[1] & 0x80) == 0x80) // exctract 0x80
				{
					// the third byte in the exception response packet is the actual exception
					switch (frame[2])
					{
						case ILLEGAL_FUNCTION: packet->illegal_function++; break;
						case ILLEGAL_DATA_ADDRESS: packet->illegal_data_address++; break;
						case ILLEGAL_DATA_VALUE: packet->illegal_data_value++; break;
						default: packet->misc_exceptions++;
					}
					messageErrFlag = 1; // set an error
					previousPolling = millis(); // start the polling delay
				}
				else // the response is valid
				{
					if (frame[1] == packet->function) // check function number returned
					{
						// receive the frame according to the modbus function
						if (packet->function == PRESET_MULTIPLE_REGISTERS) 
							check_F16_data();
						else // READ_HOLDING_REGISTERS is assumed
							check_F3_data(buffer);
					}
					else // incorrect function number returned
					{
						packet->incorrect_function_returned++; 
						messageErrFlag = 1; // set an error
						previousPolling = millis(); // start the polling delay
					} 
				} // check exception response
			} 
			else // incorrect id returned
			{
				packet->incorrect_id_returned++; 
				messageErrFlag = 1; // set an error
				previousPolling = millis(); // start the polling delay
			}
		} // check buffer
	} // check message booleans
}

// checks the time out and polling delay and if a message has been recieved succesfully 
void check_packet_status()
{
  unsigned char pollingFinished = (millis() - previousPolling) > polling;

  if (messageOkFlag && pollingFinished) // if a valid message was recieved and the polling delay has expired clear the flag
  {
    messageOkFlag = 0;
    packet->successful_requests++; // transaction sent successfully
    packet->retries = 0; // if a request was successful reset the retry counter
    transmission_ready_Flag = 1; 
  }  
	
  // if an error message was recieved and the polling delay has expired clear the flag
  if (messageErrFlag && pollingFinished) 
  {
    messageErrFlag = 0; // clear error flag 
    packet->retries++;
    transmission_ready_Flag = 1;
  } 
 	
  // if the timeout delay has past clear the slot number for next request
  if (!transmission_ready_Flag && ((millis() - previousTimeout) > timeout)) 
  {
    packet->timeout++;
    packet->retries++;
    transmission_ready_Flag = 1; 
  }
    
  // if the number of retries have reached the max number of retries 
  // allowable, stop requesting the specific packet
  if (packet->retries == retry_count)
	{
    packet->connection = 0;
		packet->retries = 0;
	}
		
	if (transmission_ready_Flag)
	{
		// update the total_errors atribute of the 
		// packet before requesting a new one
		packet->total_errors = packet->timeout + 
													 packet->incorrect_id_returned +
													 packet->incorrect_function_returned +
													 packet->incorrect_bytes_returned +
													 packet->checksum_failed +
													 packet->buffer_errors +
													 packet->illegal_function +
													 packet->illegal_data_address +
													 packet->illegal_data_value;
	}
}

void check_F3_data(unsigned char buffer)
{
	unsigned char no_of_registers = packet->no_of_registers;
  unsigned char no_of_bytes = no_of_registers * 2;
  if (frame[2] == no_of_bytes) // check number of bytes returned
  {
    // combine the crc Low & High bytes
    unsigned int recieved_crc = ((frame[buffer - 2] << 8) | frame[buffer - 1]); 
    unsigned int calculated_crc = calculateCRC(buffer - 2);
				
    if (calculated_crc == recieved_crc) // verify checksum
    {
      unsigned char index = 3;
			for (unsigned char i = 0; i < no_of_registers; i++)
      {
        // start at the 4th element in the recieveFrame and combine the Lo byte 
				packet->register_array[i] = (frame[index] << 8) | frame[index + 1]; 
        index += 2;
      }
      messageOkFlag = 1; // message successful
    }
    else // checksum failed
    {
      packet->checksum_failed++; 
      messageErrFlag = 1; // set an error
    }
      
    // start the polling delay for messageOkFlag & messageErrFlag
    previousPolling = millis(); 
  }
  else // incorrect number of bytes returned  
  {
    packet->incorrect_bytes_returned++; 
    messageErrFlag = 1; // set an error
    previousPolling = millis(); // start the polling delay
  }	                     
}
  
void check_F16_data()
{
  unsigned int recieved_address = ((frame[2] << 8) | frame[3]);
  unsigned int recieved_registers = ((frame[4] << 8) | frame[5]); 
  unsigned int recieved_crc = ((frame[6] << 8) | frame[7]); // combine the crc Low & High bytes
  unsigned int calculated_crc = calculateCRC(6); // only the first 6 bytes are used for crc calculation
  
  // check the whole packet		
  if (recieved_address == packet->address && 
      recieved_registers == packet->no_of_registers && 
      recieved_crc == calculated_crc)
      messageOkFlag = 1; // message successful
  else
  {
    packet->checksum_failed++; 
    messageErrFlag = 1;
  }
						
  // start the polling delay for messageOkFlag & messageErrFlag
  previousPolling = millis();
}

// get the serial data from the buffer
unsigned char getData()
{
  unsigned char buffer = 0;
	unsigned char overflowFlag = 0;
		
  while (Serial.available())
  {
		// The maximum number of bytes is limited to the serial buffer size of 128 bytes
		// If more bytes is received than the BUFFER_SIZE the overflow flag will be set and the 
		// serial buffer will be red untill all the data is cleared from the receive buffer,
		// while the slave is still responding.
		if (overflowFlag) 
			Serial.read();
		else
		{
			if (buffer == BUFFER_SIZE)
				overflowFlag = 1;
				
			frame[buffer] = Serial.read();
			buffer++;
		}
      
    delayMicroseconds(T1_5); // inter character time out
  }
	
  // The minimum buffer size from a slave can be an exception response of 5 bytes 
  // If the buffer was partialy filled clear the buffer.
	// The maximum number of bytes in a modbus packet is 256 bytes.
	// The serial buffer limits this to 128 bytes.
  // If the buffer overflows than clear the buffer and set
  // a packet error.
  if ((buffer > 0 && buffer < 5) || overflowFlag)
  {
    buffer = 0;
    packet->buffer_errors++; 
    messageErrFlag = 1; // set an error
    previousPolling = millis(); // start the polling delay 
  }
	
  return buffer;
}

void modbus_configure(long baud, unsigned int _timeout, unsigned int _polling, 
										unsigned char _retry_count, unsigned char _TxEnablePin, 
										Packet* _packet, unsigned int _total_no_of_packets)
{
  Serial.begin(baud);
  
  if (_TxEnablePin > 1) 
  { // pin 0 & pin 1 are reserved for RX/TX. To disable set _TxEnablePin < 2
    TxEnablePin = _TxEnablePin; 
    pinMode(TxEnablePin, OUTPUT);
    digitalWrite(TxEnablePin, LOW);
  }
	
	// Modbus states that a baud rate higher than 19200 must use a fixed 750 us 
  // for inter character time out and 1.75 ms for a frame delay.
  // For baud rates below 19200 the timeing is more critical and has to be calculated.
  // E.g. 9600 baud in a 10 bit packet is 960 characters per second
  // In milliseconds this will be 960characters per 1000ms. So for 1 character
  // 1000ms/960characters is 1.04167ms per character and finaly modbus states an
  // intercharacter must be 1.5T or 1.5 times longer than a normal character and thus
  // 1.5T = 1.04167ms * 1.5 = 1.5625ms. A frame delay is 3.5T.
	
	if (baud > 19200)
	{
		T1_5 = 750; 
		T3_5 = 1750; 
	}
	else 
	{
		T1_5 = 15000000/baud; // 1T * 1.5 = T1.5
		T3_5 = 35000000/baud; // 1T * 3.5 = T3.5
	}
	
	// initialize connection status of each packet
	for (unsigned char i = 0; i < _total_no_of_packets; i++)
	{
		_packet->connection = 1;
		_packet++;
	}
	
	// initialize
	transmission_ready_Flag = 1;
	messageOkFlag = 0; 
  messageErrFlag = 0;
  timeout = _timeout;
  polling = _polling;
	retry_count = _retry_count;
	TxEnablePin = _TxEnablePin;
	total_no_of_packets = _total_no_of_packets;
  previousTimeout = 0; 
  previousPolling = 0; 
} 

unsigned int calculateCRC(unsigned char bufferSize) 
{
  unsigned int temp, temp2, flag;
  temp = 0xFFFF;
  for (unsigned char i = 0; i < bufferSize; i++)
  {
    temp = temp ^ frame[i];
    for (unsigned char j = 1; j <= 8; j++)
    {
      flag = temp & 0x0001;
      temp >>= 1;
      if (flag)
        temp ^= 0xA001;
    }
  }
  // Reverse byte order. 
  temp2 = temp >> 8;
  temp = (temp << 8) | temp2;
  temp &= 0xFFFF;
  return temp; // the returned value is already swopped - crcLo byte is first & crcHi byte is last
}

void sendPacket(unsigned char bufferSize)
{
	if (TxEnablePin > 1)
		digitalWrite(TxEnablePin, HIGH);
		
	for (unsigned char i = 0; i < bufferSize; i++)
		Serial.write(frame[i]);
		
	Serial.flush();
	
	// allow a frame delay to indicate end of transmission
	delayMicroseconds(T3_5); 
	
	if (TxEnablePin > 1)
		digitalWrite(TxEnablePin, LOW);
		
	previousTimeout = millis(); // initialize timeout delay	
}