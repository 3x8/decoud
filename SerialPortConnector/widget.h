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

    void on_serialSelector_currentTextChanged(const QString &arg1);
    void on_serialConnect_clicked();
    void on_serialPassthough_clicked();

    void on_loadBinary_clicked();
    void on_writeBinary_clicked();
    void on_verifyBinary_clicked();

    void on_initMotor2_clicked();
    void on_initMotor1_clicked();
    void on_initMotor3_clicked();
    void on_initMotor4_clicked();

  private:
    bool parseMSPMessage = true;
    uint8_t retries = 0;
    uint8_t max_retries = 16;

    Ui::Widget *ui;
    void loadBinFile();
    void serialInfo();
    void serialPortOpen();
    void frameUploadHide(bool b);
    void frameMotorHide(bool b);

    void allup();
    bool connectMotor(uint8_t motor);
    void sendFirstEeprom();
    void serialPortClose();
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
      ACK_KO,
      ACK_CRC
    };

  private:
    FourWayIF *four_way = nullptr;
    OutConsole *msg_console = nullptr;
    QSerialPort *serial = nullptr;
    QLabel *statuslabel = nullptr;
    QByteArray *input_buffer = nullptr;
    QString filename;
    QByteArray *eeprom_buffer = nullptr;
};
