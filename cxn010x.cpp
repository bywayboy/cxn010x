#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include "HexDump.h"

#include "cxn010x.h"

#define I2C_SONY_CXNProjector   0x77

// 部分参考
// https://learn.adafruit.com/adafruit-seesaw-atsamd09-breakout/reading-and-writing-data

bool CXN_Send_Command(uint8_t * cmd, int sz){
  Wire.beginTransmission(I2C_SONY_CXNProjector);
  Wire.write(cmd, sz);
  return 0 == Wire.endTransmission();
}

void EEPROMDump(){
  int sz = 0x0C;
  uint8_t data[sz];
  for(int i=0;i<sz;i++){
    data[i] = EEPROM.read(i);
  }
  HexDump(Serial, data, sz);
}


CXNProjector::CXNProjector():stat(STATE_POWER_OFF){
  act = ACTION_NONE;
  m_HueU =m_HueV = m_SaturationU = m_SaturationV = m_Sharpness = m_Brightness = m_Contrast = 0;
  m_busy = m_Mute = false;
  stat = STATE_POWER_OFF;
}

CXNProjector::~CXNProjector(){

}

//                      0.7  0.75, 0.8   0.85, 0.9, 0.95 1.0
uint8_t pwm_speed [] = {180, 191, 204, 217, 229, 240, 255}; //风扇速度
uint8_t pwm_temp [] =  {30,   32, 34,   36, 38,   40,  42}; //温度

// 当 CMD_REQ 引脚 = 1 读取通知
void CXNProjector::OnNotify() {
  uint8_t data[32];
  int num = this->ReadNotify(data, 32);
  if(num ==0){
    return;
  }

  switch(data[0]){
    case 0x00:
      this->OnBootNotify(&data[0], num);
      break;
    case 0x01:  //开启输入
      if(data[1] == 0x01 && data[2] == 0x00){
        this->stat = STATE_ACTIVE;
      }
      break;
    case 0x02: // 关闭输入
      if(data[2] == 0x00) { //停止输入成功.
        switch(stat){
        case STATE_BOOT_READY_OFF:
          this->Shutdown(false);
          break;
        case STATE_BOOT_READY_REBOOT:
          this->Shutdown(true);
        case STATE_READY_OPTICAL:
          this->EasyOpticalAxisSet();
          break;
        case STATE_READY_BIPHASE:
          this->EasyBiphaseSet();
          break;
        default:
          stat = STATE_READY;
          break;
        }
      }
      break;
    case 0x0B:
      m_busy = false;
      if(data[1] == 0x01 && data[2] == 0x00){
        // 正常关机 或者重启.
        delay(80);
        this->PowerOff();
      }
      break;
    case 0xA0:  //光机获取温度结果通知.
      m_busy = false;
      if(data[2] == 0x00 && 0xFF != data[3]){
        uint8_t tp = data[3], fan_speed = 0x00;
        for(uint8_t i= 6; i >=0; i--){
          if(tp >= pwm_temp[i]){
            fan_speed = pwm_speed[i];
            break;
          }
        }
        //analogWrite(CXNProjector_FAN_PIN, fan_speed);  //设定风扇速度
      }
      break;
    case 0x32:  //进入光轴校准.
      if(0x00 == data[2]){
        stat = STATE_OPTICAL;
      }else{
        this->StartInput();
      }
      break;
    case 0x35:  //退出光轴校准
      this->StartInput();
      break;
    case 0x36:  //进入相位校准
      if(0x00 == data[2]){
        stat = STATE_BIPHASE;
      }else{
        this->StartInput();
      }
      break;
    case 0x39:  //退出相位校准
      this->StartInput();
      break;
    case 0x10:  //系统检测到异常. 紧急停机 并发送此通知.
      switch(data[2]){
      case 0x80:  // 检测到激光安全模块异常, 执行紧急停机.
        break;
      case 0x81:  // 固件内部发生错误, 执行紧急停机.
        break;
      case 0x82:  // 激光异常, 紧急停机.
        break;
      case 0x83: // UnderFlow occurred and recovery processing performed. 如果恢复没有问题,将继续.
        break;
      }
      break;
    case 0x11:  // 温度紧急通知, 温度异常恢复通知
      if(data[1]==0x01 && data[2]== 0x80){
        stat = STATE_MUTE;
      }else if(data[1]==0x01 && data[2]==0x00){
        stat = STATE_ACTIVE;
      }
      break;
    case 0x12: //命令处理异常发送此通知.
      //命令发送速度过快 等...
      break;
    case 0x25:  //获取所有图像位置信息.
      if(0x0A==data[1] && 0x00 == data[2]){
        m_Pan = (int8_t)data[3];
        m_Tilt = (int8_t)data[4];
        m_Flip = (int8_t)data[5];
        if(act == ACTION_LOAD_DEFAULT){
          this->SaveConfig();
          act = ACTION_NONE;
          this->StartInput();
        }
      }
      break;
    case 0x26://设置所有图像位置信息.
      if(0x01 == data[1] && (0x00==data[2] || 0xFC==data[2])){ // 数据长度 1 字节
        if(act == ACTION_INIT_CONFIG){
          this->StartInput();
        }
      }
      break;
    case 0x40: // 获取所有图像质量信息.
      if(0x0A == data[1] && 0x00==data[2]){ //数据长度 10 字节
        m_Contrast    = (int8_t)data[3];  //OP2
        m_Brightness  = (int8_t)data[4];  //OP3
        m_HueU        = (uint8_t)data[5];
        m_HueV        = (uint8_t)data[6];
        m_SaturationU = (uint8_t)data[7];
        m_SaturationV = (uint8_t)data[8];
        m_Sharpness   = (uint8_t)data[10];//锐度(激光光斑大小.)
        if(act == ACTION_LOAD_DEFAULT){
          this->GetVideoPosition(); //继续获取所有位置信息
        }
      }
      break;
    case 0x41: // 设置所有图像质量信息
      if(0x01 == data[1] && (0x00==data[2] || 0xFC==data[2])){ // 数据长度 1 字节
        if(act == ACTION_INIT_CONFIG) {
          act = ACTION_NONE;
          this->StartInput();
        }
      }
      break;
    case 0xCA: //获取故障信息通知结果.
      break;
    case 0xCB: //清除故障信息通知结果.
      break;
  }
}

// 开机状态,TODO: 处理引导通知,如果有异常发生,清除异常.
void CXNProjector::OnBootNotify(uint8_t * data, int num) {
  if(0x00==data[0]){
    switch (data[2])
    {
    case 0x00:
      stat = STATE_READY;
      
      if(! this->LoadConfig()){
        act = ACTION_LOAD_DEFAULT;  //需要加载并保存光机默认配置.
        this->GetAllPictureQualityInfo();
      }else {
        act = ACTION_INIT_CONFIG;
        this->SetVideoPosition();
        delay(20);
        this->SetAllPictureQualityInfo();
      }
      return;
    case 0x80:  // 发生内部故障,不能工作.
      break;
    case 0x81:case 0x82:case 0x83:case 0x84: // 内部故障.
      break;
    default:
      break;
    }
    this->GetTrubleInfo();
  }
}

void CXNProjector::PowerOn() {
  if(STATE_POWER_OFF == stat){
    stat = STATE_POWER_ON;
    analogWrite(CXNProjector_POWER_PIN, 0xFF);
    analogWrite(CXNProjector_FAN_PIN,   0xFF);
  }
}

void CXNProjector::PowerOff() {
  if(stat == STATE_BOOT_READY_OFF){
    analogWrite(CXNProjector_POWER_PIN, 0x00);  //断开光机电源
    analogWrite(CXNProjector_FAN_PIN,0x00);     //断开风扇电源
    stat = STATE_POWER_OFF;
  }
}

bool CXNProjector::StartInput()
{
  if(STATE_ACTIVE != stat){
    uint8_t cmd[] = {0x01, 0x00}; 
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
  return false;
}

bool CXNProjector::StopInput()
{
  uint8_t cmd[] = {0x02, 0x00}; 
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}


bool CXNProjector::Shutdown(bool isReboot)
{

  if(m_busy)
    return false;

  if(stat == STATE_ACTIVE){
    stat = isReboot?STATE_BOOT_READY_REBOOT:STATE_BOOT_READY_OFF;
    return this->StopInput();
  }else if(stat == STATE_READY || stat == STATE_BOOT_READY_OFF || stat == STATE_BOOT_READY_REBOOT){
    uint8_t cmd[] = {0x0B, 0x01, 0x00}; 
    cmd[2] = isReboot? 0x01:0x00;
    return (m_busy = (0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]))));
  }
}

bool CXNProjector::GetTrubleInfo()
{
  uint8_t cmd[] = {0xCA, 0x05, 0x01, 0x24, 0x10, 0x06, 0x00};
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}


bool CXNProjector::ClearTrubleInfo()
{
  uint8_t cmd []= {0xCB, 0x05, 0x01, 0x24, 0x10, 0x10, 0x06, 0x00, 0x00};
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

//获取所有图像质量信息
bool CXNProjector::GetAllPictureQualityInfo()
{
  uint8_t cmd[] = {0x40, 0x00}; 
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::SetAllPictureQualityInfo()
{
  uint8_t cmd[] = {0x41, 0x09, (uint8_t)m_Contrast,(uint8_t)m_Brightness, (uint8_t)m_HueU, (uint8_t)m_HueV, (uint8_t)m_SaturationU, (uint8_t)m_SaturationV, 0x00, (uint8_t)m_Sharpness, 0x00};
  Serial.println("SetAllPictureQualityInfo");
  HexDump(Serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
  delay(50);
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::SetLight(int8_t val) {
  m_Brightness += val;
  if(m_Brightness < -31){
    m_Brightness = -31;
    return false;
  }

  if(m_Brightness > 31){
    m_Brightness = 31;
    return false;
  }
  uint8_t cmd[] = {0x43, 0x01, (uint8_t)m_Brightness};
  //HexDump(Serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  return true;
}


bool CXNProjector::GetTemperature () {
  uint8_t cmd[] = {0xA0, 00};
  if(m_busy)
    return false;

  switch(stat){
    case STATE_ACTIVE:
    case STATE_READY:
      return (m_busy = (0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]))));
    default:
      break;
  }
  return true;
}

//设置锐度
bool CXNProjector::SetSharp(int8_t val)
{
  uint8_t cmd[] = {0x4F, 0x01, 0x00};

  if(val < 0)val = 0;else if(val > 6) val = 6;
  cmd[3] = (uint8_t)(0xFF & val);
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}


// 设置对比度
bool CXNProjector::SetContrast(int8_t val)
{
  uint8_t cmd[] = {CXNProjector_CMD_SET_CONTRAST, 0x01, 0x00};
  if(val < -15)val = 0;else if(val > 15) val = 15;
 
  cmd[2] = (uint8_t)(0xFF & val);
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::Mute()
{
  uint8_t cmd[] = {0x03, 0x01, 0x00};
  m_Mute = !m_Mute;
  cmd[2] = m_Mute? 0x01:0x00;
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

// 设置饱和度
bool CXNProjector::SetSaturation(int8_t U, int8_t V)
{
  m_SaturationU = (uint8_t)(0xFF & (int8_t)max(-15, min(15, U)));
  m_SaturationV = (uint8_t)(0xFF & (int8_t)max(-15, min(15, V)));

  uint8_t cmd[] = {0x49, 0x02, m_SaturationU,  m_SaturationV };
  
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

// 设置色度
bool CXNProjector::SetHue(int8_t U, int8_t V)
{
  m_HueU = (uint8_t)(0xFF & (int8_t)max(-15, min(15, U)));
  m_HueV = (uint8_t)(0xFF & (int8_t)max(-15, min(15, V)));

  uint8_t cmd[] = {0x47, 0x02, m_HueU, m_HueV };
  
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::GetVideoPosition() {
  uint8_t cmd[2] = {0x25, 0x00 };
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::SetVideoPosition(){
  uint8_t cmd[11] = {0x26, 0x09, (uint8_t)m_Pan, (uint8_t)m_Tilt, (uint8_t)m_Flip, 0x64, 0x00, 0x00, 0x00, 0x00 ,0x00};
  
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::EasyOpticalAxisSet()
{
  if(stat == STATE_ACTIVE){
    stat = STATE_READY_OPTICAL;
    this->StopInput();
  }else{
    uint8_t cmd[2] = {0x32, 0x00};
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
}

bool CXNProjector::OpticalAxisPlus(){
  if(stat == STATE_OPTICAL){
    uint8_t cmd[2] = {0x33, 0x00};
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
  return false;
}

bool CXNProjector::OpticalAxisMinus(){
  if(stat == STATE_OPTICAL){
    uint8_t cmd[2] = {0x34, 0x00};
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
  return false;
}

bool CXNProjector::EasyOpticalAxisExit(uint8_t save){
  if(stat == STATE_OPTICAL){
    uint8_t cmd[3] = {0x35, 0x01, 0x00};
    cmd[2] = save==0?0x01:0x00;
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
  return false;
}

bool CXNProjector::EasyBiphaseSet(){
  if(stat == STATE_ACTIVE){
    stat = STATE_READY_BIPHASE;
    return this->StopInput();  
  }
  uint8_t cmd[2] = {0x36, 0x00};
  return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
}

bool CXNProjector::BiphasePlus(){
  if(stat == STATE_BIPHASE){
    uint8_t cmd[2] = {0x37, 0x00};
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
  return false;
}

bool CXNProjector::BiphaseMinus(){
  if(stat == STATE_BIPHASE){
    uint8_t cmd[2] = {0x38, 0x00};
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
}

bool CXNProjector::EasyBiphaseExit(uint8_t save){
  if(stat == STATE_BIPHASE){
    uint8_t cmd[3] = {0x39, 0x01,0x00};
    cmd[2] = save==0?0x01:0x00;
    return 0 == CXN_Send_Command(cmd, sizeof(cmd) / sizeof(cmd[0]));
  }
  return false;
}

//图像翻转
bool CXNProjector::SetFlip(){
  m_Flip++;
  if(m_Flip>3) m_Flip = 0;
  return this->SetVideoPosition();
}
//左右梯形校正
bool CXNProjector::SetPan(int8_t pan){
  m_Pan += pan;
  if(m_Pan > 30) {
    m_Pan = 30; return false;
  }
  if(m_Pan < -30) {
    m_Pan = -30; return false;
  }
  return this->SetVideoPosition();
}
  // 上下梯形校正
bool CXNProjector::SetTilt(int8_t tilt){
  m_Tilt += tilt;
  if(m_Tilt > 30) {
    m_Tilt = 30; return;
  }
  if(m_Tilt < -20) {
    m_Tilt = -20; return;
  }
  return this->SetVideoPosition();
}


int CXNProjector::ReadNotify(uint8_t * data, int num)
{
   int i, ret = Wire.requestFrom(I2C_SONY_CXNProjector, num, true);
   for(int i=0; i < ret; i ++)
      data[i] = Wire.read();
   if(0 == ret){
    Serial.println("No Data");
    delay(30);
    return 0;
  }
  ret = 2 + data[1];
  HexDump(Serial, data, ret);
  delay(3);
  return ret;
}

void CXNProjector::SaveConfig() {
  EEPROM.write(0x00, (byte)0x55);
  EEPROM.write(0x01, (byte)0xAA);
  EEPROM.write(0x02, (byte)m_Contrast);
  EEPROM.write(0x03, (byte)m_Brightness);
  EEPROM.write(0x04, (byte)m_HueU);
  EEPROM.write(0x05, (byte)m_HueV);
  EEPROM.write(0x06, (byte)m_SaturationU);
  EEPROM.write(0x07, (byte)m_SaturationV);
  EEPROM.write(0x08, (byte)m_Sharpness);
  EEPROM.write(0x09, (byte)m_Pan);
  EEPROM.write(0x0A, (byte)m_Tilt);
  EEPROM.write(0x0B, (byte)m_Flip);
  Serial.println("Save EEPROM");
}

bool CXNProjector::LoadConfig() {
  if(0x55 == EEPROM[0x00] && 0xAA ==EEPROM[0x01]){
    m_Contrast    = (int8_t)EEPROM[0x02];
    m_Brightness  = (int8_t)EEPROM[0x03];
    m_HueU        = (int8_t)EEPROM[0x04];
    m_HueV        = (int8_t)EEPROM[0x05];
    m_SaturationU = (int8_t)EEPROM[0x06];
    m_SaturationV = (int8_t)EEPROM[0x07];
    m_Sharpness   = (int8_t)EEPROM[0x08];
    m_Pan         = (int8_t)EEPROM[0x09];
    m_Tilt        = (int8_t)EEPROM[0x0A];
    m_Flip        = (uint8_t)EEPROM[0x0B];
    m_Brightness = 0x01;
    Serial.println("Load EEPROM");
    return true;
  }
  return false;
}
