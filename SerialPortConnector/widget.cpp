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
    this->setWindowTitle(__REVISION__);


    QFont font("Arial");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(8);
    QApplication::setFont(font);

    serialInfo();
    connect(msg_console, &OutConsole::getData, this, &Widget::writeData);

    ui->serialConnect->setStyleSheet("background-color: red;");
    frameMotorHide(false);
    frameUploadHide(true);
    ui->writeBinary->setHidden(true);
    motorAllBlack();
}

Widget::~Widget() {
  delete ui;
}


void Widget::on_serialConnect_clicked() {
  if (serial->isOpen()) {
    serialPortClose();
  } else {
    QMessageBox::information(this, tr("information"), "remove esc power");
    serialPortOpen();
    QByteArray passthroughenable;
    four_way->passthroughStarted = true;
    parseMSPMessage = false;
    send_mspCommand(0xf5,passthroughenable);
    QMessageBox::information(this, tr("information"), "connect esc power");
  }
}

void Widget::serialPortOpen() {
  serial->setPortName(ui->serialSelector->currentText());
  serial->setBaudRate(serial->Baud115200);
  serial->setDataBits(serial->Data8);
  serial->setParity(serial->NoParity);
  serial->setStopBits(serial->OneStop);
  serial->setFlowControl(serial->NoFlowControl);

  if (serial->open(QIODevice::ReadWrite)) {
    ui->serialConnect->setText("close");
    ui->serialConnect->setStyleSheet("background-color: green;");
    showStatusMessage(tr("connected to %1 : %2, %3, %4, %5, %6").arg(serial->portName()).arg(serial->dataBits()).arg(serial->baudRate()).arg(serial->parity()).arg(serial->stopBits()).arg(serial->flowControl()));
  } else {
    //QMessageBox::critical(this, tr("Error"), serial->errorString());
    showStatusMessage(tr("open error"));
  }
}

void Widget::serialPortClose() {
  if (serial->isOpen()) {
    serial->close();
    ui->serialConnect->setText("open");
    ui->serialConnect->setStyleSheet("background-color: red;");
    showStatusMessage(tr("disconnected"));

    frameMotorHide(false);
    frameUploadHide(true);
    ui->writeBinary->setHidden(true);
    motorAllBlack();
  }
}

void Widget::loadBinFile() {
  filename = QFileDialog::getOpenFileName(this, tr("open file"), "c:", tr("all files (*.*)"));

  ui->writeBinary->setHidden(false);
}

void Widget::showStatusMessage(const QString &message) {
  ui->serialLabel->setText(message);
}

void Widget::serialInfo() {
    const auto infos = QSerialPortInfo::availablePorts();
    QString s;

    for (const QSerialPortInfo &info : infos) {
      ui->serialSelector->addItem(info.portName());
    }

}

void Widget::readData() {
  const QByteArray data = serial->readAll();

  qInfo("size of data : %d ", data.size());
  QByteArray data_hex_string = data.toHex();

  if (four_way->passthroughStarted) {
    if (four_way->ackRequired == true) {
      if (four_way->checkCRC(data,data.size())) {
        if (data[data.size() - 3] == (char) 0x00) {     // ACK OK!!
          four_way->ackRequired = false;
          four_way->ackType = ACK_OK;
          ui->statusLabel->setText("esc->ACK_OK");

          // if verifying flash
          if (data[1] == (char) 0x3a) {
            input_buffer->clear();
            for(int i = 0; i < (uint8_t)data[4]; i ++){
              input_buffer->append(data[i+5]); // first 4 byte are package header
            }
            qInfo("esc->ACK_OK");
          }

          if (data[1] == (char) 0x37) {
            four_way->escConnected = true;
            ui->statusLabel->setText("esc->connected");
            frameUploadHide(false);
          }
        } else {
          // bad ack
          if (data[1] == (char) 0x37) {
            four_way->escConnected = false;
            frameUploadHide(true);
          }
          four_way->ackType = ACK_KO;
          ui->statusLabel->setText("esc->ACK_KO");
          qInfo("esc->ACK_KO");
        }
      } else {
        qInfo("4WAY CRC ERROR");
        ui->statusLabel->setText("esc->ACK_KO");
        four_way->ackType = ACK_CRC;
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

void Widget::on_serialSelector_currentTextChanged(const QString &arg1) {
  UNUSED(arg1);
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
    for  (int j = 0; j < 4; j++){      // 4 buffers per page
      QByteArray twofiftysixitems;
      // for debugging limit to 50
      for (int k = 0; k < 256; k++) {       // transfer 256 bytes each buffer
        twofiftysixitems.append(line.at(k+ (i*1024) + (j*256)));
        index++;
        if(index >= sizeofBin){
          break;
        }
      }

      four_way->ackRequired = true;
      retries = 0;
      while (four_way->ackRequired) {
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

      if (four_way->ackType == ACK_KO) {
        return;
      }

      if (four_way->ackType == ACK_CRC) {
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

bool Widget::motorConnect(uint8_t motor) {
    uint16_t buffer_length = 48;
    four_way->ackRequired = true;
    uint8_t retries = 0;

    while(four_way->ackRequired) {
      writeData(four_way->makeFourWayCommand(0x37,motor));
      serial->waitForBytesWritten(500);
      while (serial->waitForReadyRead(500)) {
        // noop
      }
      readData();
      while (serial->waitForReadyRead(50)) {
        // noop
      }

      retries++;
      if (retries > 2) {
        return (false);
      }
    }

    if (four_way->escConnected == false) {
      qInfo("esc->notConnected");
      ui->statusLabel->setText("esc->notConnected");
      return (false);
    }

    four_way->ackRequired = true;

    while (four_way->ackRequired) {
      writeData(four_way->makeFourWayReadCommand(buffer_length,0x7c00));
      serial->waitForBytesWritten(500);
      while (serial->waitForReadyRead(500)) {
        // noop
      }

      readData();
      retries++;
      if (retries > 2) {
        return (false);
      }
    }

    ui->statusLabel->setText("esc->connected");
    return (true);

    /*
    if (input_buffer->at(0) == 0x01) {
      // EEPROM->OK
    } else {
      // EEPROM->KO
    }*/
}

void Widget::motorAllBlack() {
  ui->initMotor1->setStyleSheet("color: black;");
  ui->initMotor2->setStyleSheet("color: black;");
  ui->initMotor3->setStyleSheet("color: black;");
  ui->initMotor4->setStyleSheet("color: black;");

  ui->progressBar->setValue(0);
}

void Widget::on_initMotor1_clicked() {
  motorAllBlack();
  if (motorConnect(0x00)) {
    //ui->initMotor1->setFont(this->font());
    ui->initMotor1->setStyleSheet("color: green;");
  } else {
    ui->initMotor1->setStyleSheet("color: white;");
  }
}

void Widget::on_initMotor2_clicked() {
  motorAllBlack();
  if (motorConnect(0x01)) {
    ui->initMotor2->setStyleSheet("color: green;");
  } else {
    ui->initMotor2->setStyleSheet("color: white;");
  }
}

void Widget::on_initMotor3_clicked() {
  motorAllBlack();
  if (motorConnect(0x02)) {
    ui->initMotor3->setStyleSheet("color: green;");
  } else {
    ui->initMotor3->setStyleSheet("color: white;");
  }
}

void Widget::on_initMotor4_clicked() {
  motorAllBlack();
  if(motorConnect(0x03)){
    ui->initMotor4->setStyleSheet("color: green;");
  } else {
    ui->initMotor4->setStyleSheet("color: white;");
  }
}

void Widget::frameMotorHide(bool b) {
  ui->frameMotor->setHidden(b);
}

void Widget::frameUploadHide(bool b) {
  ui->frameUpload->setHidden(b);
}
