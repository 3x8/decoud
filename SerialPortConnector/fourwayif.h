#pragma once

#include <QMessageBox>

class FourWayIF {
  public:
    bool checkCRC(const QByteArray data , uint16_t buffer_length);
    bool ACK_required();
    //bool ACK_received();
    void set_Ack_req(char ackreq);
    bool ack_required;
    bool ack_received;
    uint8_t ack_type;
    bool passthrough_started;
    bool ESC_connected;

    FourWayIF();
    QByteArray makeFourWayWriteCommand(const QByteArray sendbuffer, int buffer_size, uint16_t address);
    QByteArray makeFourWayReadCommand( int buffer_size, uint16_t address );
    QByteArray makeFourWayCommand(uint8_t cmd, uint8_t device_num);
    uint16_t makeCRC(const QByteArray data);

  private:
    int testVar;
    char ack_req;

};
