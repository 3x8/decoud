#include "widget.h"
#include "ui_widget.h"
#include "outconsole.h"
#include "fourwayif.h"
#include "mspif.h"

#include <QFile>
#include <QTextStream>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QMessageBox>
#include <QScrollBar>
#include <QComboBox>
#include <QTimer>
#include <QFileDialog>

Widget::Widget(QWidget *parent): QWidget(parent),
  ui(new Ui::Widget),
  four_way(new FourWayIF),
  msg_console(new OutConsole),
  serial(new QSerialPort(this)),
  input_buffer(new QByteArray),
  eeprom_buffer(new QByteArray) {

    ui->setupUi(this);
    this->setWindowTitle("Multi ESC Config Tool 0.1");
    serialInfo();
    connect(msg_console, &OutConsole::getData, this, &Widget::writeData);

    frameMotorHide(false);
    frameUploadHide(true);

    ui->writeBinary->setHidden(true);
    ui->VerifyFlash->setHidden(true);
}

Widget::~Widget() {
  delete ui;
}


void Widget::on_buttonConnect_clicked() {
  if (serial->isOpen()) {
    serialPortClose();
  } else {
    serialPortOpen();
  }
}

void Widget::on_buttonPassthough_clicked() {
  if (four_way->passthrough_started == false) {

    QByteArray passthroughenable2;    // payload  empty here
    four_way->passthrough_started = true;
    parseMSPMessage = false;
    send_mspCommand(0xf5,passthroughenable2);
    ui->buttonPassthough->setText("passthrough stop");
  } else {

    writeData(four_way->makeFourWayCommand(0x34,0x00));
    parseMSPMessage = true;
    four_way->passthrough_started = false;
    ui->buttonPassthough->setText("passthrough start");
  }



}

void Widget::serialPortOpen() {
  serial->setPortName(ui->serialSelectorBox->currentText());
  serial->setBaudRate(serial->Baud115200);
  serial->setDataBits(serial->Data8);
  serial->setParity(serial->NoParity);
  serial->setStopBits(serial->OneStop);
  serial->setFlowControl(serial->NoFlowControl);

  if (serial->open(QIODevice::ReadWrite)) {
    ui->buttonConnect->setText("close");
    ui->buttonConnect->setStyleSheet("background-color: rgb(255,0,0);");
    showStatusMessage(tr("connected to %1 : %2, %3, %4, %5, %6").arg(serial->portName()).arg(serial->dataBits()).arg(serial->baudRate()).arg(serial->parity()).arg(serial->stopBits()).arg(serial->flowControl()));
  } else {
    //QMessageBox::critical(this, tr("Error"), serial->errorString());
    showStatusMessage(tr("open error"));
  }
}

void Widget::serialPortClose() {
  if (serial->isOpen()) {
    serial->close();
    ui->buttonConnect->setText("open");
    ui->buttonConnect->setStyleSheet("background-color: rgb(0,255,0);");
    showStatusMessage(tr("disconnected"));
  }
}

void Widget::loadBinFile() {
  filename = QFileDialog::getOpenFileName(this,
  tr("open file"), "c:", tr("all files (*.*)"));

  ui->writeBinary->setHidden(false);
  ui->VerifyFlash->setHidden(false);
}

void Widget::showStatusMessage(const QString &message) {
  ui->label_2->setText(message);
}

void Widget::serialInfo() {
    const auto infos = QSerialPortInfo::availablePorts();
    QString s;

    for (const QSerialPortInfo &info : infos) {      // here we should add to drop down menu
      ui->serialSelectorBox->addItem(info.portName());
      //  serial->setPortName(info.portName());
      s = s + QObject::tr("port: ") + info.portName() + "\n"
            + QObject::tr("location: ") + info.systemLocation() + "\n"
            + QObject::tr("description: ") + info.description() + "\n"
            + QObject::tr("manufacturer: ") + info.manufacturer() + "\n"
            + QObject::tr("serialNumber: ") + info.serialNumber() + "\n"
            + QObject::tr("vendorIdentifier: ") + (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()) + "\n"
            + QObject::tr("productIdentifier: ") + (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()) + "\n";
    }

}



void Widget::readData() {
  const QByteArray data = serial->readAll();

  qInfo("size of data : %d ", data.size());
  QByteArray data_hex_string = data.toHex();

  if (four_way->passthrough_started) {
    if (four_way->ack_required == true) {
      if (four_way->checkCRC(data,data.size())) {
        if (data[data.size() - 3] == (char) 0x00) {     // ACK OK!!
          four_way->ack_required = false;
          four_way->ack_type = ACK_OK;
          ui->StatusLabel->setText("esc->ACK_OK");
          if (data[1] == (char) 0x3a) {
            // if verifying flash
            input_buffer->clear();
            for(int i = 0; i < (uint8_t)data[4]; i ++){
              input_buffer->append(data[i+5]); // first 4 byte are package header
            }
            qInfo("esc->ACK_OK");
          }
          if(data[1] == (char) 0x37){
            frameUploadHide(false);
            ui->StatusLabel->setText("Connected");
            four_way->ESC_connected = true;
          }
        } else {            // bad ack
          if(data[1] == (char) 0x37){
            frameUploadHide(true);
            four_way->ESC_connected = false;
          }

          qInfo("esc->ACK_KO");
          ui->StatusLabel->setText("esc->ACK_KO");
          four_way->ack_type = ACK_KO;
          // four_way->ack_required = false;
        }
      } else {
        qInfo("4WAY CRC ERROR");
        ui->StatusLabel->setText("esc->ACK_KO");
        four_way->ack_type = ACK_CRC;
      }
    } else {
       qInfo("no ack required");
    }
  }
  /*
  if (parseMSPMessage) {
    data[]         // 24
    data[]         // 4d
    data[]         // 3c
    data[]         // length  one byte
    data[]         // command one byte
    data[]         // payload x length bytes
    data[]         // crc
  }*/
}

void Widget::writeData(const QByteArray &data) {
  serial->write(data);
}

uint8_t Widget::mspSerialChecksumBuf(uint8_t checksum, const uint8_t *data, int len) {
  while (len-- > 0) {
    checksum ^= *data++;
  }
  return (checksum);
}



void Widget::on_serialSelectorBox_currentTextChanged(const QString &arg1) {
  // noop
}

void Widget::send_mspCommand(uint8_t cmd, QByteArray payload) {
  QByteArray mspMsgOut;
  mspMsgOut.append((char) 0x24);
  mspMsgOut.append((char) 0x4d);
  mspMsgOut.append((char) 0x3c);
  mspMsgOut.append((char) payload.length());
  mspMsgOut.append((char) cmd);
  if(payload.length() > 0){
    mspMsgOut.append(payload);
  }

  uint8_t checksum = 0;
  for(int i = 3; i < mspMsgOut.length(); i++){
    checksum ^= mspMsgOut[i];
  }
  mspMsgOut.append((char) checksum);
  writeData(mspMsgOut);
}

void Widget::on_loadBinary_clicked() {
  loadBinFile();
}

void Widget::on_writeBinary_clicked() {
  QFile inputFile(filename);
  inputFile.open(QIODevice::ReadOnly);
  QByteArray line = inputFile.readAll();
  QByteArray data_hex_string = line.toHex();

  inputFile.close();

  uint32_t sizeofBin = line.size();
  uint16_t index = 0;
  ui->progressBar->setValue(0);
  uint8_t pages = sizeofBin / 1024;
  uint8_t max_retries = 8;
  uint8_t retries = 0;

  for (int i = 0; i <= pages; i++){   // for each page ( including partial page at end)
    for(int j = 0; j < 4; j++){      // 4 buffers per page
      QByteArray twofiftysixitems;
      // for debugging limit to 50
      for (int k = 0; k < 256; k++) {       // transfer 256 bytes each buffer
        twofiftysixitems.append(line.at(k+ (i*1024) + (j*256)));
        index++;
        if(index >= sizeofBin){
          break;
        }
      }

      four_way->ack_required = true;
      retries = 0;
      while (four_way->ack_required) {
        writeData(four_way->makeFourWayWriteCommand(twofiftysixitems, twofiftysixitems.size(), 8192 + (i*1024) + (j*256))) ;     // increment address every i and j
        serial->waitForBytesWritten(1000);
        serial->waitForReadyRead(250);
        readData();
        qInfo("what is going on? size :  %d ", ((uint8_t)twofiftysixitems[0]));

        retries++;
        if (retries > max_retries) {        // after 8 tries to get an ack
          break;
        }
      }

      if (four_way->ack_type == ACK_KO) {
        return;
      }

      if (four_way->ack_type == ACK_CRC) {
        i--;// go back to beggining of page to erase in case data has been written.
        index = (index - 256 * j);
        break;
      }

      ui->progressBar->setValue((index*100) / sizeofBin);
      QApplication::processEvents();
      if(index >= sizeofBin){
        break;
      }
    }
  }
  qInfo("what is going on? size :  %d ", sizeofBin );
}

void Widget::on_VerifyFlash_clicked() {
    QFile inputFile(filename);
    inputFile.open(QIODevice::ReadOnly);

    //  QTextStream in(&inputFile);
    QByteArray line = inputFile.readAll();
    inputFile.close();

    uint16_t bin_size = line.size();
    uint16_t K128Chunks = bin_size / 128;
    uint32_t index = 0;
    for (int i = 0; i < K128Chunks +1; i++) {
      retries = 0;
      four_way->ack_required = true;
      while (four_way->ack_required) {
        writeData(four_way->makeFourWayReadCommand(128,  4096+ (i*128)));
        serial->waitForBytesWritten(500);
        while (serial->waitForReadyRead(500)) {
          // noop
        }
        readData();
        retries++;
        if (retries>max_retries) {        // after 8 tries to get an ack
          return;
        }
      }

      for (int j = 0; j < input_buffer->size(); j ++) {
        if (input_buffer->at(j) == line.at(j + i * 128)) {
          qInfo("the same! index : %d", index);
          index++;
          if(index>=bin_size){
            qInfo("all memory verified in flash memory");
            break;
          }
        } else {
          qInfo("data error in flash memory");
          return;
        }
      }

      ui->progressBar->setValue((index*100)/bin_size);
      QApplication::processEvents();
    }

    //  four_way->ack_required = true;
    //  writeData(four_way->makeFourWayReadCommand(128,  4096 + i*128));
}

bool Widget::connectMotor(uint8_t motor) {
    uint16_t buffer_length = 48;
    four_way->ack_required = true;
    retries = 0;

    writeData(four_way->makeFourWayCommand(0x37,motor));
    serial->waitForBytesWritten(500);
    while (serial->waitForReadyRead(500)) {
      // noop
    }
    readData();
    while (serial->waitForReadyRead(50)) {
      // noop
    }

    //retries++;
    //if(retries>max_retries/2){        // after 8 tries to get an ack
    //    return (false);
    //}

    if (four_way->ESC_connected == false) {
      qInfo("esc->notConnected");
      ui->StatusLabel->setText("esc->notConnected");
      return (false);
    }

    four_way->ack_required = true;

    while (four_way->ack_required) {
      writeData(four_way->makeFourWayReadCommand(buffer_length,0x7c00));
      serial->waitForBytesWritten(500);
      while (serial->waitForReadyRead(500)) {
        // noop
      }

      readData();
      retries++;
      if (retries>max_retries/4) {        // after 8 tries to get an ack
        return (false);
      }
    }

    if (input_buffer->at(0) == 0x01) {
      ui->StatusLabel->setText("esc->connected EEPROM->OK");
      return (true);
    } else {
      ui->StatusLabel->setText("esc->connected EEPROM->KO");
      return (false);
    }
}

void Widget::allup() {
  ui->initMotor1->setFlat(false);
  ui->initMotor2->setFlat(false);
  ui->initMotor3->setFlat(false);
  ui->initMotor4->setFlat(false);
}

void Widget::on_initMotor1_clicked() {
  allup();
  ui->initMotor1->setFlat(true);

  if (connectMotor(0x00)) {
    //ui->StatusLabel->setText("M1:Connected: Settings read OK ");
  }
}

void Widget::on_initMotor2_clicked() {
  allup();
  ui->initMotor2->setFlat(true);

  if (connectMotor(0x01)) {
    //ui->StatusLabel->setText("M2:Connected: Settings read OK ");
  }
}

void Widget::on_initMotor3_clicked() {
  allup();
  ui->initMotor3->setFlat(true);

  if (connectMotor(0x02)) {
    //ui->StatusLabel->setText("M3:Connected: Settings read OK ");
  }
}

void Widget::on_initMotor4_clicked() {
  allup();
  ui->initMotor4->setFlat(true);

  if(connectMotor(0x03)){
    //ui->StatusLabel->setText("M4:Connected: Settings read OK ");
    return;
  }
}

void Widget::frameMotorHide(bool b) {
  ui->frameMotor->setHidden(b);
}

void Widget::frameUploadHide(bool b) {
  ui->frameUpload->setHidden(b);
}
