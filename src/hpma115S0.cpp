/**
 * @file HPMA115S0.cpp
 * @author Felix Galindo
 * @date June 2017
 * @brief Arduino Library for Honeywell's Particle Sensor (HPMA115S0-XXX)
 * @license MIT
 */

#include "Arduino.h"
#include "HPMA115S0.h"

#define DEBUG_HPMA115S0 true

extern "C" {
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}

/**
 * @brief Constructor for HPMA115S0 class
 * @param  a Stream ({Software/Hardware}Serial) object.
 * @note The serial stream should be already initialized
 * @return  void
 */
HPMA115S0::HPMA115S0(Stream& serial):
  _serial(serial)
{
  _serial.setTimeout(100);
}

/**
 * @brief Function that initializes sensor
 * @return  a String containing sensor response
 **/
void HPMA115S0::Init() {
  if (DEBUG_HPMA115S0) Serial.println("PS- Initializing...");
  delay(100);
  StartParticleMeasurement();
  delay(100);
  DisableAutoSend();
}

/**
 * @brief Function that sends serial command to sensor
 * @param  a unsigned char * containing the command
 * @param size of buffer
 * @return  void
 */

void HPMA115S0::SendCmd(const char * constCmdBuf, unsigned short cmdSize) {
  
  //Clear RX
  while (_serial.available())
    _serial.read();

  //Send command
  if (DEBUG_HPMA115S0)Serial.print("PS- Sending cmd: ");
  unsigned short index = 0;
  for (index = 0; index < cmdSize; index++) {
    if (DEBUG_HPMA115S0) Serial.print(cmdBuf[index], HEX);
    if (DEBUG_HPMA115S0) Serial.print(" ");
    _serial.write(cmdBuf[index]);
  }
  if (DEBUG_HPMA115S0) Serial.println("");
  return;
}

/**
 * @brief Function that reads command response from sensor
 * @param Buffer to store data in
 * @param Buffer size
 * @param Expected command type
 * @return  returns number of bytes read from sensor
 */
short HPMA115S0::ReadCmdResp(unsigned char * dataBuf, unsigned short dataBufSize, unsigned short cmdType) {
  static unsigned char respBuf[HPM_MAX_RESP_SIZE];
  static unsigned short respIdx = 0;
  static unsigned short calChecksum = 0;

  //Read response
  respIdx = 0;
  calChecksum = 0;
  memset(respBuf, 0, sizeof(respBuf));
  _serial.setTimeout(100);
  if (DEBUG_HPMA115S0) Serial.println("PS- Waiting for cmd resp...");
  if (_serial.readStringUntil(HPM_CMD_RESP_HEAD)) {
    delay(1); //wait for the rest of the bytes to arrive
    respBuf[HPM_HEAD_IDX] = HPM_CMD_RESP_HEAD;
    respBuf[HPM_LEN_IDX] = _serial.read(); //Read the command length

	if (DEBUG_HPMA115S0) Serial.println("PS- Read completed");

    //Ensure buffers are big enough
    if (respBuf[HPM_LEN_IDX] && ((respBuf[HPM_LEN_IDX] + 1) <=  sizeof(respBuf) - 2) && (respBuf[HPM_LEN_IDX] - 1) <= dataBufSize ) {
      if (_serial.readBytes(&respBuf[HPM_CMD_IDX], respBuf[HPM_LEN_IDX] + 1) == (respBuf[HPM_LEN_IDX] + 1)) { //read respBuf[HPM_LEN_IDX] num of bytes + calChecksum byte
        if (respBuf[HPM_CMD_IDX] == cmdType) { //check if CMD type matches

          //Calculate and validate checksum
          for (respIdx = 0; respIdx < (2 + respBuf[HPM_LEN_IDX]); respIdx++) {
            calChecksum += respBuf[respIdx];
          }
          calChecksum = (65536 - calChecksum) % 256;
          if (calChecksum == respBuf[2 + respBuf[HPM_LEN_IDX]]) {
            if (DEBUG_HPMA115S0) Serial.println("PS- Received valid data!!!");
            memset(dataBuf, 0, dataBufSize);
            memcpy(dataBuf, &respBuf[HPM_DATA_START_IDX], respBuf[HPM_LEN_IDX] - 1);
            return (respBuf[HPM_LEN_IDX] - 1);
          }
        }
		else
		{
			if (DEBUG_HPMA115S0) Serial.println("PS- Wrong cmdType");
		}
      }
	  else
	  {
		if (DEBUG_HPMA115S0) Serial.println("PS- _serial.readBytes incorrect number of bytes");
	  }
    }
	else
	{
		if (DEBUG_HPMA115S0) Serial.println("PS- Buffer overflow");
	}
  }
  if (DEBUG_HPMA115S0) Serial.println("PS- Timeout while waiting for cmd resp");
  return false;
}


/**
 * @brief Function that sends a read command to sensor
 * @return  returns true if valid measurements were read from sensor
 */
boolean HPMA115S0::ReadParticleMeasurement(unsigned short * pm2_5, unsigned short * pm10) {
  const char cmdBuf[] = {0x68, 0x01, 0x04, 0x93};
  static unsigned char dataBuf[HPM_READ_PARTICLE_MEASURMENT_LEN_C - 1];
  int len;

  if (DEBUG_HPMA115S0) Serial.println("PS- Reading Particle Measurements..." );

  //Send command
  SendCmd(cmdBuf, 4);

  //Read response
  len = ReadCmdResp(dataBuf, sizeof(dataBuf), READ_PARTICLE_MEASURMENT);
  if ((len == (HPM_READ_PARTICLE_MEASURMENT_LEN - 1)) || (len == (HPM_READ_PARTICLE_MEASURMENT_LEN_C - 1))) {

    if (len == (HPM_READ_PARTICLE_MEASURMENT_LEN - 1)) {
      // HPMA115S0 Standard devices
      _pm2_5 = dataBuf[0] * 256 + dataBuf[1];
      _pm10 = dataBuf[2] * 256 + dataBuf[3];
    } else {
      // HPMA115C0 Compact devices
      _pm2_5 = dataBuf[2] * 256 + dataBuf[3];
      _pm10 = dataBuf[6] * 256 + dataBuf[7];
    } 
    *pm2_5 = _pm2_5;
    *pm10 = _pm10;
    // Serial.println("PS- PM 2.5: " + String(_pm2_5) + " ug/m3" );
    // Serial.println("PS- PM 10: " + String(_pm10) + " ug/m3" );
    return true;
  }
  return false;
}

/**
 * @brief Function that starts sensor measurement
 * @return  void
 */
void HPMA115S0::StartParticleMeasurement() {
  const char cmd[] = {0x68, 0x01, 0x01, 0x96};
  SendCmd(cmd, 4);
}

/**
 * @brief Function that stops sensor measurement
 * @return  void
 */
void HPMA115S0::StopParticleMeasurement() {
  const char cmd[] = {0x68, 0x01, 0x02, 0x95};
  SendCmd(cmd, 4);
}

/**
 * @brief Function that enables auto send
 * @return  void
 */
void HPMA115S0::EnableAutoSend() {
  const char cmd[] = {0x68, 0x01, 0x40, 0x57};
  SendCmd(cmd, 4);
}

/**
 * @brief Function that stops auto send
 * @return  void
 */
void HPMA115S0::DisableAutoSend() {
  const char cmd[] = {0x68, 0x01, 0x20, 0x77};
  SendCmd(cmd, 4);
}

/**
* @brief Function that returns the latest PM 2.5 reading
* @note Sensor reports new reading ~ every 1 sec.
* @return  PM 2.5 reading (unsigned int)
*/
unsigned short HPMA115S0::GetPM2_5() {
  return _pm2_5;
}

/**
* @brief Function that returns the latest PM 10 reading
* @note Sensor reports new reading ~ every 1 sec.
* @return  PM 10 reading (unsigned int)
*/
unsigned short HPMA115S0::GetPM10() {
  return _pm10;
}

unsigned short HPMA115S0::getAdjustmentCoefficient() {
  const char cmd[] = {0x68, 0x01, 0x10, 0x87};
  SendCmd(cmd, 4);

  static unsigned char dataBuf[HPM_READ_ADJUSTMENT_COEFFICIENT_LEN - 1];
  
  if (ReadCmdResp(dataBuf, sizeof(dataBuf), READ_ADJUSTMENT_COEFFICIENT) == (HPM_READ_ADJUSTMENT_COEFFICIENT_LEN - 1)) {
    _coefficiency = dataBuf[0];
    return _coefficiency;
  }

  return 0;
}

void HPMA115S0::setAdjustmentCoefficient(unsigned short coefficient)
{
  const char cmd[] = {0x68, 0x02, 0x8, coefficient};
  SendCmd(cmd, 3);
}