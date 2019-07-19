#include <IRremote.h>
//#include <avr/sleep.h>
#include <Wire.h>

#include "cxn010x.h"
#include "HexDump.h"

/*
 * 主板: Arduino Nano
 * 引导: Atmega328P(Old bootloader)
 *  引脚定义表:
      IIC1 PA4 SDA    PA5 SCL
*/


#define LED               13
#define CMD_REQ_PIN       PD2  // PD0 INT0
#define VOLTAGE_PIN       25  // PA2
#define RECV_PIN          6   // 红外遥控

IRrecv irrecv(RECV_PIN);
decode_results results;
CXNProjector projector;

uint8_t DEVICE_IS_LOCKED = 0;

void setup() {

  pinMode(LED, OUTPUT);
  pinMode(CXNProjector_POWER_PIN, OUTPUT);
  pinMode(CMD_REQ_PIN, INPUT);
  pinMode(RECV_PIN, INPUT);
  
  irrecv.blink13(true);
  irrecv.enableIRIn();

  digitalWrite(CXNProjector_POWER_PIN, LOW); //开机状态 不给光机供电.
  DEVICE_IS_LOCKED = 0;

  Serial.begin(115200);
  Wire.begin(); //初始化I2C
  attachInterrupt(0, OnCXNProjectorSingle, RISING);   //上升沿触发.
}

// 通知引脚变化, 调用到实例中.
void OnCXNProjectorSingle(void) {
  noInterrupts(); //关闭中断源
  // 循环读取,直到CMD_REQ 电平拉低了
  do {
    projector.OnNotify();
  } while(HIGH == digitalRead(CMD_REQ_PIN));
  interrupts(); //开启中断源
}

// 读取多少次取平均值. 增加准确度.
float ReadVoltage(int num){
  float voltage = 0.0;
  int i = 0;
  for(i=0;i<num;i++){
    voltage += analogRead(VOLTAGE_PIN) *  (5.0 / 1023.0);
  }
  return voltage / i;
}

CXNProjector_State stat = STATE_POWER_ON;
int sret = 0;
void loop() {
  
  CXNProjector_State cur = projector.GetState();
  if(cur !=  stat){
    switch(cur){
     case STATE_POWER_OFF:
      Serial.println("POWER_OFF");break;
     case STATE_POWER_ON:
      Serial.println("POWER_ON");break;
     case STATE_READY:
      Serial.println("READY");break;
     case STATE_ACTIVE:
      Serial.println("ACTIVE");break;
     case STATE_MUTE:
      Serial.println("MUTE");break;
    }
  }
  stat = cur;

  if (irrecv.decode(&results)) {
    if (results.decode_type == NEC) {
      if(results.value != -1) {
        switch(results.value) {
          case 0xFFA25D:  // Power ON (CH-) 自动 Start Input
            Serial.println("Power ON");
            break;
          case 0xFF629D:  // Mute (CH)
            Serial.println("Mute");
            break;
          case 0xFFE21D:  // CH+ 左右梯形
            break;
          case 0xFF22DD: // |<<  亮度
            break;
          case 0xFF02FD: // >>|  对比度
            break;
          case 0xFFC23D: // > |  对比度
            break;
          case 0xFF906F: // EQ  上下梯形
            break;
          case 0xFFA857: // +
            break;
          case 0xFFE01F: // -
            break;
          case 0xFF6897: // 0   
            break;
          case 0xFF9867: // 100+
            break;
          case 0xFFB04F: // 200+
            break;
          case 0xFF30CF:  // 1
            break;
          case 0xFF18E7:  // 2
            break;
          case 0xFF7A85:  // 3
            break;
          case 0xFF10EF:  // 4
            break;
          case 0xFF38C7:  // 5
            break;
          case 0xFF5AA5:  // 6
            break;
          case 0xFF42BD:  // 7
            break;
          case 0xFF4AB5:  // 8
            break;
          case 0xFF52AD:  // 9
            break;
        }
      }
    }
    irrecv.resume(); // Receive the next value
  }

/*
    //机器锁定了 啥也别干了.
    if(DEVICE_IS_LOCKED)
      return;
    // 光机断电状态. 
    if(digitalRead(CXNProjector_POWER_PIN) == 0) {
      float voltage = ReadVoltage(3);
      if(voltage >= 4.7){
        projector.PowerOn();
      }
    } else {
      // 工作状态 电压低于安全阀值 强行断电并锁定.
      float voltage = ReadVoltage(10);
      if(voltage < 4.0 ){
        DEVICE_IS_LOCKED = 1; // 标记设备锁定
        projector.PowerOff();      // 光机强行断电.
      }
    }
*/
}
