#include <dummy.h>
#include <StreamUtils.h>
#include "main.h"
//#include <SoftwareSerial.h>

//extern SoftwareSerial Serial2; // veriable
// SoftwareSerial Serial2(RXPin, TXPin);
/**
 * Flush the RS485 buffers in both directions. The doc for Serial.flush() implies it only
 * flushes outbound characters now... I assume Serial2 is the same.
 */
void flushRS485()
{
	Serial2.flush();
	tdelay(200);

	while(Serial2.available())
		Serial2.read();
}
/////////////////////////////////////////////////////////////////////////////////////
//
//
/////////////////////////////////////////////////////////////////////////////////////
int sendModbus_f(uint8_t frame[], byte frameSize, modbusResponse *resp)
{
	//Calculate the CRC and overwrite the last two bytes.
	calcCRC(frame, frameSize);

	// Make sure there are no spurious characters in the in/out buffer.
	flushRS485();

	//Send
	//digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_TX);
	Serial2.write(frame, frameSize);

	// It's important to reset the SERIAL_COMMUNICATION_CONTROL_PIN as soon as
	// we finish sending so that the serial port can start to buffer the response.
	//digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_RX);
	return listen(resp);
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Listen for a response.
/////////////////////////////////////////////////////////////////////////////////////////
int listen(modbusResponse *resp)
{
	uint8_t		inFrame[64];
	uint8_t		inByteNum = 0;
	uint8_t		inFrameSize = 0;
	uint8_t		inFunctionCode = 0;
	uint8_t		inDataBytes = 0;
	int		done = 0;
	modbusResponse	dummy;

	if(!resp)
		resp = &dummy;      // Just in case we ever want to interpret here.

	resp->dataSize = 0;
	resp->errorLevel = 0;

	while((!done) && (inByteNum < sizeof(inFrame)))
	{
		int tries = 0;

		while((!Serial2.available()) && (tries++ < RS485_TRIES))
			tdelay(50);

		if(tries >= RS485_TRIES)
		{
			Serial.println("Timeout waiting for RS485 response.");
			break;
		}

		inFrame[inByteNum] = Serial2.read();

		//Process the byte
		switch(inByteNum)
		{
			case 0:
				if(inFrame[inByteNum] != SOFAR_SLAVE_ID)   //If we're looking for the first byte but it dosn't match the slave ID, we're just going to drop it.
					inByteNum--;          // Will be incremented again at the end of the loop.
				break;

			case 1:
				//This is the second byte in a frame, where the function code lives.
				inFunctionCode = inFrame[inByteNum];
				break;

			case 2:
				//This is the third byte in a frame, which tells us the number of data bytes to follow.
				if((inDataBytes = inFrame[inByteNum]) > sizeof(inFrame))
				inByteNum = -1;       // Frame is too big?
				break;

			default:
				if(inByteNum < inDataBytes + 3)
				{
					//This is presumed to be a data byte.
					resp->data[inByteNum - 3] = inFrame[inByteNum];
					resp->dataSize++;
				}
				else if(inByteNum > inDataBytes + 3)
					done = 1;
		}

		inByteNum++;
	}

	inFrameSize = inByteNum;

	/*
	* Now check to see if the last two bytes are a valid CRC.
	* If we don't have a response pointer we don't care.
	**/
	if(inFrameSize < 5) 
	{
		resp->errorLevel = 2;
    strcpy(resp->errorMessage,"Response too short");
		//resp->errorMessage = "Response too short";
	}
	else if(checkCRC(inFrame, inFrameSize))
	{
		resp->errorLevel = 0;
     strcpy(resp->errorMessage,"Valid data frame");
	//	resp->errorMessage = "Valid data frame";
	}
	else
	{
		resp->errorLevel = 1;
     strcpy(resp->errorMessage,"Error: invalid data frame");
		//resp->errorMessage = "Error: invalid data frame";
	}

	if(resp->errorLevel)
		Serial.println(resp->errorMessage);

	return -resp->errorLevel;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////////////////////////
int readSingleReg(uint8_t id, uint16_t reg, modbusResponse *rs)
{
	uint8_t	frame[] = { id, MODBUS_FN_READSINGLEREG, reg >> 8, reg & 0xff, 0, 0x01, 0, 0 };

	return sendModbus_f(frame, sizeof(frame), rs);
}

////////////////////////////////////////////////////////////////////////////////////////////
//
// instruction for inverter
///////////////////////////////////////////////////////////////////////////////////////////////
int sendPassiveCmd(uint8_t id, uint16_t cmd, uint16_t param, const char* pubTopic)
{
	modbusResponse	rs;
	uint8_t	frame[] = { id, SOFAR_FN_PASSIVEMODE, cmd >> 8, cmd & 0xff, param >> 8, param & 0xff, 0, 0 };
	int		err = -1;
	char	retMsg[50];

	if(sendModbus_f(frame, sizeof(frame), &rs))
  strcpy(retMsg, rs.errorMessage);
	//	retMsg = rs.errorMessage;
	else if(rs.dataSize != 2)
  {
    sprintf(retMsg,"Reponse is %d bytes?",rs.dataSize);
    strcpy(retMsg, rs.errorMessage);
	//	retMsg = "Reponse is " + String(rs.dataSize) + " bytes?";
  }
	else
	{
    sprintf(retMsg,"%d", (rs.data[0] << 8) | (rs.data[1] & 0xff) );
    strcpy(retMsg, rs.errorMessage);
	//	retMsg = String((rs.data[0] << 8) | (rs.data[1] & 0xff));
		err = 0;
	}

#ifdef DEBUG
 Serial.println("RS422 set "+String(pubTopic)+" returned " + String(retMsg));
#endif
if(err == 0)
  updateOLED(NULL, NULL,pubTopic , NULL); 
	return err;
}

///////////////////////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////////////////////////
int addStateInfo(String &state, uint16_t reg, String human)
{
	unsigned int	val;
	modbusResponse	rs;

	if(readSingleReg(SOFAR_SLAVE_ID, reg, &rs))
		return -1;

	val = ((rs.data[0] << 8) | rs.data[1]);

	if (!( state == "{"))
		state += ",";

	state += "\"" + human + "\":" + String(val);
	return 0;
}


//calcCRC and checkCRC are based on...
//https://github.com/angeloc/simplemodbusng/blob/master/SimpleModbusMaster/SimpleModbusMaster.cpp

void calcCRC(uint8_t frame[], byte frameSize) 
{
	unsigned int temp = 0xffff, flag;

	for(unsigned char i = 0; i < frameSize - 2; i++)
	{
		temp = temp ^ frame[i];

		for(unsigned char j = 1; j <= 8; j++)
		{
			flag = temp & 0x0001;
			temp >>= 1;

			if(flag)
				temp ^= 0xA001;
		}
	}

	// Bytes are reversed.
	frame[frameSize - 2] = temp & 0xff;
	frame[frameSize - 1] = temp >> 8;
}

bool checkCRC(uint8_t frame[], byte frameSize) 
{
	unsigned int calculated_crc, received_crc;

	received_crc = ((frame[frameSize-2] << 8) | frame[frameSize-1]);
	calcCRC(frame, frameSize);
	calculated_crc = ((frame[frameSize-2] << 8) | frame[frameSize-1]);
	return (received_crc = calculated_crc);
}
