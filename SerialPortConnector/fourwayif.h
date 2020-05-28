#pragma once

#include <QMessageBox>

class FourWayIF {
  public:
    bool checkCRC(const QByteArray data , uint16_t buffer_length);
    bool ackRequired;
    uint8_t ackType;
    bool passthroughStarted;
    bool escConnected;

    FourWayIF();
    QByteArray makeFourWayWriteCommand(const QByteArray sendbuffer, int buffer_size, uint16_t address);
    QByteArray makeFourWayReadCommand( int buffer_size, uint16_t address );
    QByteArray makeFourWayCommand(uint8_t cmd, uint8_t device_num);
    uint16_t makeCRC(const QByteArray data);

  private:

};
