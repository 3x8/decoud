#pragma once

#include <QMessageBox>

class FourWayIF {
  public:
    bool ackRequired;
    uint8_t ackType;
    bool passthroughStarted;
    bool escConnected;

    FourWayIF();
    QByteArray makeFourWayWriteCommand(const QByteArray bufferSend, int bufferSize, uint16_t address);
    QByteArray makeFourWayEraseAllCommand();
    QByteArray makeFourWayErasePageCommand(uint8_t page);
    QByteArray makeFourWayReadCommand( int bufferSize, uint16_t address );
    QByteArray makeFourWayCommand(uint8_t command, uint8_t deviceNumber);
    uint16_t makeCRC(const QByteArray data);
    bool checkCRC(const QByteArray data , uint16_t buffer_length);

  private:

};
