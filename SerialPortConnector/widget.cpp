#include "widget.h"
#include "ui_widget.h"
#include "fourwayif.h"

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
  fourWay(new FourWayIF),
  serial(new QSerialPort(this)),
  bufferInput(new QByteArray),
  bufferEeprom(new QByteArray) {

    ui->setupUi(this);
    sprintf(version,"costaguana v0.0.1 (%s)",__REVISION__);
    this->setWindowTitle(version);

    QFont font("Arial");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(8);
    QApplication::setFont(font);

    serialInfo();

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
    fourWay->passthroughStarted = true;
    parseMSPMessage = false;
    mspCommandSend(0xf5,passthroughenable);
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
    ui->statusLabel->setText("disconnected");
    motorAllBlack();
  }
}

void Widget::loadBinFile() {
  filename = QFileDialog::getOpenFileName(this, tr("open file"), "c:", tr("all files (*.*)"));

  if(filename.size() > 3) {
    ui->writeBinary->setHidden(false);
  } else {
    ui->writeBinary->setHidden(true);
  }
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

  qInfo("data.size() = %d ", data.size());

  if (fourWay->passthroughStarted) {
    if (fourWay->ackRequired == true) {
      if (fourWay->checkCRC(data,data.size())) {
        if (data[data.size() - 3] == (char) 0x00) {     // ACK OK!!
          fourWay->ackRequired = false;
          fourWay->ackType = ACK_OK;
          ui->statusLabel->setText("esc->ACK_OK");

          // if verifying flash
          /*
          if (data[1] == (char) 0x3a) {
            bufferInput->clear();
            for(int i = 0; i < (uint8_t)data[4]; i ++){
              bufferInput->append(data[i+5]); // first 4 byte are package header
            }
            qInfo("esc->ACK_OK");
          }*/

          if (data[1] == (char) 0x37) {
            fourWay->escConnected = true;
            ui->statusLabel->setText("esc->connected");
            frameUploadHide(false);
          }
        } else {
          // bad ack
          if (data[1] == (char) 0x37) {
            fourWay->escConnected = false;
            frameUploadHide(true);
          }
          fourWay->ackType = ACK_KO;
          ui->statusLabel->setText("esc->ACK_KO");
          qInfo("esc->ACK_KO");
        }
      } else {
        fourWay->ackType = ACK_CRC;
        ui->statusLabel->setText("esc->ACK_CRC");
        qInfo("esc->ACK_CRC");
      }
    } else {
       qInfo("fourWay->ackRequired = false");
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

void Widget::mspCommandSend(uint8_t command, QByteArray payload) {
  QByteArray mspMsgOut;
  mspMsgOut.append((char) 0x24);
  mspMsgOut.append((char) 0x4d);
  mspMsgOut.append((char) 0x3c);
  mspMsgOut.append((char) payload.length());
  mspMsgOut.append((char) command);
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
  inputFile.close();

  uint32_t sizeofBin = line.size();
  uint16_t index = 0;
  ui->progressBar->setValue(0);
  uint8_t pages = sizeofBin / 1024;
  uint8_t retriesIndex = 0;

  for (int i = 0; i <= pages; i++){   // for each page ( including partial page at end)
    for  (int j = 0; j < 4; j++){      // 4 buffers per page
      QByteArray buffer256;
      // for debugging limit to 50
      for (int k = 0; k < 256; k++) {       // transfer 256 bytes each buffer
        buffer256.append(line.at(k + (i*1024) + (j*256)));
        index++;
        if(index >= sizeofBin){
          break;
        }
      }

      fourWay->ackRequired = true;
      retriesIndex = 0;
      while (fourWay->ackRequired) {
        writeData(fourWay->makeFourWayWriteCommand(buffer256, buffer256.size(), 8192 + (i*1024) + (j*256))) ;     // increment address every i and j
        serial->waitForBytesWritten(1000);
        serial->waitForReadyRead(250);
        readData();
        //qInfo("(uint8_t)buffer256[0] = %d ", ((uint8_t)buffer256[0]));

        retriesIndex++;
        if (retriesIndex > RETRIES_MAX) {
          break;
        }
      }

      if (fourWay->ackType == ACK_KO) {
        return;
      }

      if (fourWay->ackType == ACK_CRC) {
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
  qInfo("sizeofBin = %d ", sizeofBin );
}

bool Widget::motorConnect(uint8_t motor) {
    uint16_t buffer_length = 48;
    fourWay->ackRequired = true;
    uint8_t retriesIndex = 0;

    while(fourWay->ackRequired) {
      writeData(fourWay->makeFourWayCommand(0x37,motor));
      serial->waitForBytesWritten(500);
      while (serial->waitForReadyRead(500)) {
        // noop
      }
      readData();
      while (serial->waitForReadyRead(50)) {
        // noop
      }

      retriesIndex++;
      if (retriesIndex > RETRIES_MAX) {
        return (false);
      }
    }

    if (fourWay->escConnected == false) {
      qInfo("esc->notConnected");
      ui->statusLabel->setText("esc->notConnected");
      return (false);
    }

    fourWay->ackRequired = true;

    while (fourWay->ackRequired) {
      writeData(fourWay->makeFourWayReadCommand(buffer_length,0x7c00));
      serial->waitForBytesWritten(500);
      while (serial->waitForReadyRead(500)) {
        // noop
      }

      readData();
      retriesIndex++;
      if (retriesIndex > RETRIES_MAX) {
        return (false);
      }
    }

    ui->statusLabel->setText("esc->connected");
    return (true);

    /*
    if (bufferInput->at(0) == 0x01) {
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
