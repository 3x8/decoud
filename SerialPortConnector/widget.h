#pragma once

#include <QWidget>
#include <QSerialPort>
#include <QLabel>
#include <QComboBox>

class OutConsole;
class FourWayIF;

QT_BEGIN_NAMESPACE
  namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget {
    Q_OBJECT

  public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

  private slots:

    uint8_t mspSerialChecksumBuf(uint8_t checksum, const uint8_t *data, int len);
    void send_mspCommand(uint8_t cmd, QByteArray payload);

    void on_sendButton_clicked();
    void on_disconnectButton_clicked();
    void on_pushButton_clicked();
    void on_sendMessageButton_clicked();
    void on_passthoughButton_clicked();
    void on_horizontalSlider_sliderMoved(int position);
    void on_serialSelectorBox_currentTextChanged(const QString &arg1);
    void on_fourWaySendButton_clicked();
    void on_pushButton_2_clicked();
    void on_loadBinary_clicked();
    void on_writeBinary_clicked();
    void on_VerifyFlash_clicked();
    void on_sendMspComButton_clicked();
    void on_initMotor2_clicked();
    void on_initMotor1_clicked();
    void on_initMotor3_clicked();
    void on_initMotor4_clicked();
    void on_writeEEPROM_clicked();
    void on_sendFirstEEPROM_clicked();
    void on_devSettings_stateChanged(int arg1);
    void on_endPassthrough_clicked();

  private:
    Ui::Widget *ui;
    void loadBinFile();
    void serialInfoStuff();
    void connectSerial();
    void hide4wayButtons(bool b);
    void hideESCSettings(bool b);
    void hideEEPROMSettings(bool b);
    void allup();
    bool connectMotor(uint8_t motor);
    void sendFirstEeprom();
    void closeSerialPort();
    void readData();
    void putData(const QByteArray &data);
    void verifyFlash();
    void writeData(const QByteArray &data);
    void showStatusMessage(const QString &message);

    typedef union __attribute__ ((packed)) {
        uint8_t bytes[2];
        uint16_t word;
    } uint8_16_u;

    enum messages{
        ACK_OK,
        BAD_ACK,
        CRC_ERROR

    };

   bool parseMSPMessage = true;
   bool more_to_come = false;

   uint8_t retries = 0;
   uint8_t max_retries = 16;

//    typedef struct ioMem_s {
//        uint8_t D_NUM_BYTES;
//        uint8_t D_FLASH_ADDR_H;
//        uint8_t D_FLASH_ADDR_L;
//        uint8_t *D_PTR_I;
//    } ioMem_t;

  private:
    FourWayIF *four_way = nullptr;
    OutConsole *msg_console = nullptr;
    QSerialPort *m_serial = nullptr;
    QLabel *statuslabel = nullptr;
    QByteArray *input_buffer = nullptr;
    QString filename;
    QByteArray *eeprom_buffer = nullptr;

    //  uint8_t output_bin_data[255] = {0};
    //  QComboBox *m_box = nullptr;
    //  InConsole *rsp_console = nullptr;
};
