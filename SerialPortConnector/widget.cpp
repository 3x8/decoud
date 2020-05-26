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

    hide4wayButtons(true);
    hideESCSettings(true);
    hideEEPROMSettings(true);

    ui->writeBinary->setHidden(true);
    ui->VerifyFlash->setHidden(true);
    ui->devFrame->setHidden(true);
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
    hide4wayButtons(false);
    QByteArray passthroughenable2;    // payload  empty here
    four_way->passthrough_started = true;
    parseMSPMessage = false;
    send_mspCommand(0xf5,passthroughenable2);
    ui->buttonPassthough->setText("passthrough stop");
  } else {
    hide4wayButtons(true);
    hideESCSettings(true);
    hideEEPROMSettings(true);
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
  ui->textEdit->setPlainText(filename);
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
    ui->textEdit->setPlainText(s);
}



void Widget::readData() {
  const QByteArray data = serial->readAll();

  qInfo("size of data : %d ", data.size());
  QByteArray data_hex_string = data.toHex();
  if(ui->checkBox_2->isChecked()){
    ui->plainTextEdit_2->insertPlainText(data_hex_string);           // adds data to end of old text
    ui->plainTextEdit_2->insertPlainText("\n");
  } else {
    ui->plainTextEdit_2->appendPlainText(data);
  }

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
            hideESCSettings(false);
            hideEEPROMSettings(false);
            // QApplication::processEvents();
          }
          if(data[1] == (char) 0x37){
            hideESCSettings(false);
            ui->escStatusLabel->setText("Connected");
            four_way->ESC_connected = true;
          }
        } else {            // bad ack
          if(data[1] == (char) 0x37){
            hideESCSettings(true);
            four_way->ESC_connected = false;
          }
          hideEEPROMSettings(true);
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

void Widget::on_buttonSendMessage_clicked() {
  msg_console->setEnabled(true);
  const QByteArray data = ui->plainTextEdit->toPlainText().toLocal8Bit();
  writeData(data);
}

uint8_t Widget::mspSerialChecksumBuf(uint8_t checksum, const uint8_t *data, int len) {
  while (len-- > 0) {
    checksum ^= *data++;
  }
  return (checksum);
}

void Widget::on_buttonSetIfARM_clicked() {
  four_way->ack_required = true;
  writeData(four_way->makeFourWayCommand(0x3f,0x04));
}

void Widget::on_pushButton_2_clicked() {
  four_way->ack_required = true;
  writeData(four_way->makeFourWayCommand(0x37,0x00));
}

void Widget::on_horizontalSlider_sliderMoved(int position) {
  char highByteThrottle = (position >> 8) & 0xff;;
  char lowByteThrottle = position & 0xff;

  QString s = QString::number(position);
  ui->lineEdit->setText(s);
  // 24 4d 3c 10 d6 d0 07 d0 07 d0 07 d0 07 00 00 00 00 00 00 00 00 c6
  QByteArray sliderThrottle;
  sliderThrottle.append((char) lowByteThrottle);   // motor 1
  sliderThrottle.append((char) highByteThrottle);  //
  sliderThrottle.append((char) lowByteThrottle);   // motor 2
  sliderThrottle.append((char) highByteThrottle);
  sliderThrottle.append((char) lowByteThrottle);
  sliderThrottle.append((char) highByteThrottle);
  sliderThrottle.append((char) lowByteThrottle);
  sliderThrottle.append((char) highByteThrottle);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);
  sliderThrottle.append((char) 0x00);

  if (ui->checkBox->isChecked()) {
    send_mspCommand(0xd6,sliderThrottle);
  }
}

void Widget::on_serialSelectorBox_currentTextChanged(const QString &arg1) {
  // noop
}

void Widget::on_fourWaySendButton_clicked() {
  four_way->ack_required = true;
  if (ui->FourWayComboBox->currentText() == "device_write(3b)") {
    // save fake eeprom
    uint8_t fakeeeprom[48] = {
        0x01,              // firmware version currently version 1
        0x23, 0x4f, 0x70, 0x65, 0x6e, 0x45, 0x73, 0x63, 0x56, 0x31, 0x2e, 0x32, 0x23, 0x20, 0x20, 0x20,       // ESC name
        0x01,      // direction reversed
        0x01,      // bidectional mode 1 for on 0 for off byte 19
        0x00,      // sinusoidal startup  byte 20
        0x23, 0x4e, 0x45, 0x4f, 0x45, 0x53, 0x43, 0x23, 0x66, 0x30, 0x35, 0x31, 0x23, 0x20, 0x20, 0x20,  // up to byte 36 for future use
        0x23, 0x4e, 0x45, 0x4f, 0x45, 0x53, 0x43, 0x23, 0x66, 0x30, 0x35, 0x31  //byte 48
    };

    QByteArray fourWayMsgOut;
    fourWayMsgOut.append((char) 0x2f);
    fourWayMsgOut.append((char) 0x3b);
    fourWayMsgOut.append((char) 0x7c);
    fourWayMsgOut.append((char) 0x00);
    fourWayMsgOut.append((char) 0x30);      // message length     == 48

    for(int i = 0;i < 48; i++){
      fourWayMsgOut.append((char) fakeeeprom[i]);
    }

    uint16_t crc  =0;
    for (int i = 0; i < fourWayMsgOut.length(); i++) {
      crc = crc ^ (fourWayMsgOut[i] << 8);
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

    fourWayMsgOut.append((char) fourWayCrcHighByte);
    fourWayMsgOut.append((char) fourWayCrcLowByte);
    //  writeData(passthroughenable);
    writeData(fourWayMsgOut);
  }

  if (ui->FourWayComboBox->currentText() == "Test_Alive_30") {
    writeData(four_way->makeFourWayCommand(0x30,0x00));
    // QByteArray fourWayMsgOut;
    // fourWayMsgOut.append((char) 0x2f);
    // fourWayMsgOut.append((char) 0x30);
    // fourWayMsgOut.append((char) 0x00);
    // fourWayMsgOut.append((char) 0x00);
    // fourWayMsgOut.append((char) 0x01);
    // fourWayMsgOut.append((char) 0x00);
    // fourWayMsgOut.append((char) 0xcf);
    // fourWayMsgOut.append((char) 0xd4);
    // // writeData(passthroughenable);
    // writeData(fourWayMsgOut);
  }

  if (ui->FourWayComboBox->currentText() == "Get_Version_31") {
    writeData(four_way->makeFourWayCommand(0x31,0x00));
  }

  if (ui->FourWayComboBox->currentText() == "Exit_Interface_34") {
    hide4wayButtons(true);
    hideESCSettings(true);
    hideEEPROMSettings(true);
    writeData(four_way->makeFourWayCommand(0x34,0x00));
    parseMSPMessage = true;
    four_way->passthrough_started = false;
  }

  if (ui->FourWayComboBox->currentText() == "CheckifCrcWorking") {     // this is the crc for 1 wire if
    QByteArray checkcrcmessageout;
    checkcrcmessageout.append((char) 0xfe);
    checkcrcmessageout.append((char) 0x00);
    checkcrcmessageout.append((char) 0x00);
    checkcrcmessageout.append((char) 0x04);

    // checkcrcmessageout.append((char) 0x03);
    // checkcrcmessageout.append((char) 0x70);

    static uint8_16_u CRC_16;
    CRC_16.word=0;
    for (int i = 0; i < checkcrcmessageout.length(); i++) {
      uint8_t xb = checkcrcmessageout[i];

      for (uint8_t j = 0; j < 8; j++) {
        if (((xb & 0x01) ^ (CRC_16.word & 0x0001)) !=0 ) {
          CRC_16.word = CRC_16.word >> 1;
          CRC_16.word = CRC_16.word ^ 0xA001;
        } else {
          CRC_16.word = CRC_16.word >> 1;
        }
        xb = xb >> 1;
      }
    }

    char fourWayCrcLowByte = CRC_16.bytes[0];
    char fourWayCrcHighByte = CRC_16.bytes[1];

    checkcrcmessageout.append((char) fourWayCrcLowByte);
    checkcrcmessageout.append((char) fourWayCrcHighByte);
    writeData(checkcrcmessageout);
  }


  if (ui->FourWayComboBox->currentText() == "Device_Read(3a)") {
    writeData(four_way->makeFourWayReadCommand(48,0x7c00));
  }

  if (ui->FourWayComboBox->currentText() == "Get_IF_Name_32") {
    writeData(four_way->makeFourWayCommand(0x32,0x00));
  }

  if (ui->FourWayComboBox->currentText() == "Device_Init_flash_37") {      // only esc 00 for now
    writeData(four_way->makeFourWayCommand(0x37,0x00));
  }

  if (ui->FourWayComboBox->currentText() == "Reset_ESCs_35") {            // only esc 00 for now
    writeData(four_way->makeFourWayCommand(0x35,0x00));
  }

  if (ui->FourWayComboBox->currentText() == "Set_interface(Silabs)_3F") {            // only esc 00 for now
    writeData(four_way->makeFourWayCommand(0x3f,0x01));
  }

  if (ui->FourWayComboBox->currentText() == "Set_interface(Arm)_3F"){            // only esc 00 for now
    writeData(four_way->makeFourWayCommand(0x3f,0x04));
  }
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
  ui->plainTextEdit_2->setPlainText(line);
  ui->textEdit->setPlainText(data_hex_string);

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

void Widget::on_sendMspComButton_clicked() {
  const QString data = ui->lineEdit_5->text();
  int serint = data.toInt();
  send_mspCommand((uint8_t)serint, 0);
}

bool Widget::connectMotor(uint8_t motor) {
    uint16_t buffer_length = 48;
    four_way->ack_required = true;
    retries = 0;
    // while(four_way->ack_required){

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

    if(four_way->ESC_connected == false){
      qInfo("not connected");
      ui->escStatusLabel->setText("Can Not Connect");
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
      if(input_buffer->at(17) == 0x01){
        ui->rvCheckBox->setChecked(true);
      } else {
        ui->rvCheckBox->setChecked(false);
      }

      if (input_buffer->at(18) == 0x01) {
        ui->biDirectionCheckbox->setChecked(true);
      } else {
        ui->biDirectionCheckbox->setChecked(false);
      }

      if (input_buffer->at(19) == 0x01) {
        ui->sinCheckBox->setChecked(true);
      } else {
        ui->sinCheckBox->setChecked(false);
      }

      if (input_buffer->at(20) == 0x01) {
        ui->comp_pwmCheckbox->setChecked(true);
      } else {
        ui->comp_pwmCheckbox->setChecked(false);
      }

      if(input_buffer->at(21) == 0x01){
        ui->varPWMCheckBox->setChecked(true);
      }else{
        ui->varPWMCheckBox->setChecked(false);
      }

      QChar charas = input_buffer->at(18);
      int output = charas.toLatin1();
      qInfo(" output integer %i", output);
      eeprom_buffer->clear();
      // eeprom_buffer = input_buffer;
      for(int i = 0; i < buffer_length; i++){
        eeprom_buffer->append(input_buffer->at(i));
      }
      //  ui->escStatusLabel->setText("Connected - EEok ");
      return (true);
    } else {
      hideEEPROMSettings(true);
      ui->biDirectionCheckbox->setChecked(false);
      ui->rvCheckBox->setChecked(false);
      ui->sinCheckBox->setChecked(false);
      ui->escStatusLabel->setText("Connected - No EEprom");
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
    ui->escStatusLabel->setText("M1:Connected: Settings read OK ");
  }
}

void Widget::on_initMotor2_clicked() {
  allup();
  ui->initMotor2->setFlat(true);

  if (connectMotor(0x01)) {
    ui->escStatusLabel->setText("M2:Connected: Settings read OK ");
  }
}

void Widget::on_initMotor3_clicked() {
  allup();
  ui->initMotor3->setFlat(true);

  if (connectMotor(0x02)) {
    ui->escStatusLabel->setText("M3:Connected: Settings read OK ");
  }
}

void Widget::on_initMotor4_clicked() {
  allup();
  ui->initMotor4->setFlat(true);

  if(connectMotor(0x03)){
    ui->escStatusLabel->setText("M4:Connected: Settings read OK ");
    return;
  } else {
     ui->escStatusLabel->setText("M4:Did not connect");
  }
}

void Widget::on_writeEEPROM_clicked() {
  four_way->ack_required = true;
  QByteArray eeprom_out;
  for (int i = 0 ; i < 48 ; i++){
    eeprom_out.append(eeprom_buffer->at(i));
  }
  eeprom_out[17] = (char)ui->rvCheckBox->isChecked();
  eeprom_out[18] = (char)ui->biDirectionCheckbox->isChecked();
  eeprom_out[19] = (char)ui->sinCheckBox->isChecked();
  eeprom_out[20] = (char)ui->comp_pwmCheckbox->isChecked();
  eeprom_out[21] = (char)ui->varPWMCheckBox->isChecked();
  writeData(four_way->makeFourWayWriteCommand(eeprom_out, 48, 0x7c00)) ;

  serial->waitForBytesWritten(500);
  while (serial->waitForReadyRead(500)) {
    // noop
  }

  readData();
  if (four_way->ack_required == false) {     // good ack received from esc
    ui->escStatusLabel->setText("WRITE EEPROM SUCCESSFUL");
  }
}

void Widget::hide4wayButtons(bool b) {
  ui->fourWayFrame->setHidden(b);

}

void Widget::hideESCSettings(bool b) {
  ui->settingsFrame->setHidden(b);
  //  qInfo(" slot working");
}

void Widget::hideEEPROMSettings(bool b) {
  ui->eepromFrame->setHidden(b);
  //  qInfo(" slot working");
}

void Widget::sendFirstEeprom() {
  uint8_t starteeprom[48] = {
    0x01,              // firmware start byte, must be 1
    0x23, 0x4f, 0x70, 0x65, 0x6e, 0x45, 0x73, 0x63, 0x56, 0x31, 0x2e, 0x32, 0x23, 0x20, 0x20, 0x20,       // ESC name
    0x00,      // direction reversed
    0x00,      // bidectional mode 1 for on 0 for off byte 19
    0x00,      // sinusoidal startup  byte 20
    0x01,      //complementary pwm byte 21
    0x01,      //variable pwm frequency
    0x45,
    0x4f,
    0x45,
    0x53, 0x43, 0x23, 0x66, 0x30, 0x35, 0x31, 0x23, 0x20, 0x20, 0x20,  // up to byte 36 for future use
    0x23, 0x4e, 0x45, 0x4f, 0x45, 0x53, 0x43, 0x23, 0x66, 0x30, 0x35, 0x31  //byte 48
  };

  QByteArray eeprom_out;
  for (int i = 0 ; i < 48; i ++) {
    eeprom_out.append((char)starteeprom[i]);
  }
  four_way->ack_required = true;
  writeData(four_way->makeFourWayWriteCommand(eeprom_out, 48, 0x7c00)) ;
  serial->waitForBytesWritten(500);
  while (serial->waitForReadyRead(500)) {
    // noop
  }

  readData();
  if(four_way->ack_required == false){     // good ack received from esc
    ui->escStatusLabel->setText("WRITE DEFAULT SUCCESS");
  }
}

void Widget::on_sendFirstEEPROM_clicked() {
  sendFirstEeprom();
}

void Widget::on_devSettings_stateChanged(int arg1) {
  if(arg1 == 0) {
    ui->devFrame->setHidden(true);
  } else {
    ui->devFrame->setHidden(false);
  }
}
