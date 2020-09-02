#include "fourwayif.h"

#include <QMessageBox>

FourWayIF::FourWayIF() {
  ackRequired = true;
  ackType = 1;
  passthroughStarted = false;
  escConnected = false;
}

QByteArray FourWayIF::makeFourWayWriteCommand(const QByteArray bufferSend, int bufferSize, uint16_t address ) {
  QByteArray fourWayWriteMsgOut;

  if (bufferSize == 256) {
    bufferSize = 0;
  }

  fourWayWriteMsgOut.append((char) 0x2f);
  fourWayWriteMsgOut.append((char) 0x3b);
  fourWayWriteMsgOut.append((char) (address >> 8) & 0xff);  // address high byte
  fourWayWriteMsgOut.append((char) address & 0xff); // address low byte
  fourWayWriteMsgOut.append((char) bufferSize); // message length
  fourWayWriteMsgOut.append(bufferSend);

  uint16_t writeCrc  = makeCRC(fourWayWriteMsgOut);
  uint8_t fourWayCrcHighByte = (writeCrc >> 8) & 0xff;;
  uint8_t fourWayCrcLowByte = writeCrc & 0xff;
  fourWayWriteMsgOut.append((char) fourWayCrcHighByte);
  fourWayWriteMsgOut.append((char) fourWayCrcLowByte);

  return(fourWayWriteMsgOut);
}

QByteArray FourWayIF::makeFourWayEraseAllCommand() {
  QByteArray fourWayWriteMsgOut;

  fourWayWriteMsgOut.append((char) 0x2f);
  fourWayWriteMsgOut.append((char) 0x38);
  fourWayWriteMsgOut.append((char) 0x00);
  fourWayWriteMsgOut.append((char) 0x00);
  fourWayWriteMsgOut.append((char) 0x01);
  fourWayWriteMsgOut.append((char) 0x00);
  fourWayWriteMsgOut.append((char) 0xCD);
  fourWayWriteMsgOut.append((char) 0xF9);

  /*
  uint16_t writeCrc  = makeCRC(fourWayWriteMsgOut);
  uint8_t fourWayCrcHighByte = (writeCrc >> 8) & 0xff;;
  uint8_t fourWayCrcLowByte = writeCrc & 0xff;
  fourWayWriteMsgOut.append((char) fourWayCrcHighByte);
  fourWayWriteMsgOut.append((char) fourWayCrcLowByte);*/

  return(fourWayWriteMsgOut);
}

QByteArray FourWayIF::makeFourWayErasePageCommand(uint8_t page) {
  QByteArray fourWayWriteMsgOut;

  fourWayWriteMsgOut.append((char) 0x2f);
  fourWayWriteMsgOut.append((char) 0x39);
  fourWayWriteMsgOut.append((char) 0x00);
  fourWayWriteMsgOut.append((char) 0x00);
  fourWayWriteMsgOut.append((char) 0x01);
  fourWayWriteMsgOut.append((char) page);
  uint16_t writeCrc  = makeCRC(fourWayWriteMsgOut);
  uint8_t fourWayCrcHighByte = (writeCrc >> 8) & 0xff;;
  uint8_t fourWayCrcLowByte = writeCrc & 0xff;
  fourWayWriteMsgOut.append((char) fourWayCrcHighByte);
  fourWayWriteMsgOut.append((char) fourWayCrcLowByte);

  return(fourWayWriteMsgOut);
}

QByteArray FourWayIF::makeFourWayReadCommand(int bufferSize, uint16_t address ) {
  if (bufferSize == 256) {
    bufferSize = 0;
  }

  QByteArray fourWayReadMsgOut;
  fourWayReadMsgOut.append((char) 0x2f);
  fourWayReadMsgOut.append((char) 0x3a);
  fourWayReadMsgOut.append((char) (address >> 8) & 0xff); // address high byte
  fourWayReadMsgOut.append((char) address & 0xff);  // address low byte
  fourWayReadMsgOut.append((char) 0x01);
  fourWayReadMsgOut.append((char) bufferSize);  // message length
  uint16_t readCrc  = makeCRC(fourWayReadMsgOut);

  uint8_t fourWayCrcHighByte = (readCrc >> 8) & 0xff;;
  uint8_t fourWayCrcLowByte = readCrc & 0xff;
  fourWayReadMsgOut.append((char) fourWayCrcHighByte);
  fourWayReadMsgOut.append((char) fourWayCrcLowByte);

  return(fourWayReadMsgOut);
}

QByteArray FourWayIF::makeFourWayCommand(uint8_t command, uint8_t deviceNumber){
  QByteArray fourWayMsgOut;

  fourWayMsgOut.append((char) 0x2f);  // escape character PC
  fourWayMsgOut.append((char) command);   // 4 way command
  fourWayMsgOut.append((char) 0x00);  // address 0 for single command
  fourWayMsgOut.append((char) 0x00);  //  adress
  fourWayMsgOut.append((char) 0x01);  // payload size
  fourWayMsgOut.append((char) deviceNumber);  // payload  ESC device number

  uint16_t msgCRC = makeCRC(fourWayMsgOut);
  char fourWayCrcHighByte = (msgCRC >> 8) & 0xff;;
  char fourWayCrcLowByte = msgCRC & 0xff;
  fourWayMsgOut.append((char) fourWayCrcHighByte);
  fourWayMsgOut.append((char) fourWayCrcLowByte);

  return(fourWayMsgOut);
}

uint16_t FourWayIF::makeCRC(const QByteArray data){
  uint16_t crc  = 0;

  for (int i = 0; i < data.length(); i++) {
    crc = crc ^ (data[i] << 8);
    for (int i = 0; i < 8; ++i) {
      if (crc & 0x8000){
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
    crc = crc & 0xffff;
  }
  return (crc);
}

bool FourWayIF::checkCRC(const QByteArray data, uint16_t bufferSize){
  uint16_t crc  = 0;

  for (int i = 0; i < bufferSize - 2; i++) {
    crc = crc ^ (data[i] << 8);
    for (int i = 0; i < 8; ++i) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
    crc = crc & 0xffff;
  }

  char fourWayCrcHighByte = (crc >> 8) & 0xff;;
  char fourWayCrcLowByte = crc & 0xff;

  qInfo("fourWayCrc : %x:%x",fourWayCrcHighByte & 0xff, fourWayCrcLowByte & 0xff);
  qInfo("receivedCrc: %x:%x",data[bufferSize-2] & 0xff, data[bufferSize-1] & 0xff);

  if ((fourWayCrcHighByte == data[bufferSize-2]) && (fourWayCrcLowByte == data[bufferSize-1])) {
    return(true);
  } else {
    return(false);
  }
}
