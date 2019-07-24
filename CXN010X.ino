#include <IRremote.h>
//#include <avr/sleep.h>
#include <Wire.h>
#include <time.h>

#include "cxn010x.h"
#include "HexDump.h"

/*
 * 主板: Arduino Nano
 * 引导: Atmega328P(Old bootloader)
 *  引脚定义表:
      IIC1 PA4 SDA    PA5 SCL
*/


#define LED               13
#define CMD_REQ_PIN       2  // PD0 INT0
#define VOLTAGE_PIN       14  // PA0
#define RECV_PIN          PD6   // 红外遥控

IRrecv irrecv(RECV_PIN);
decode_results results;
CXNProjector projector;

uint8_t DEVICE_IS_LOCKED = 0;
unsigned long g_ms = 0;
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

  projector.PowerOn();
  delay(1000);
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


CXNProjector_State stat = STATE_POWER_ON;
int sret = 0;
void loop() {
  
  //这里暂时不使用中断控制
  if(digitalRead(2)==HIGH) {
    delay(10);  //等待 10ms; 如果电平还是高电平,读取通知.
    // 循环读取,直到CMD_REQ 电平拉低了
    while(HIGH == digitalRead(CMD_REQ_PIN)) {
      projector.OnNotify();
    }
  }

  if(millis() - g_ms > 1000){
    g_ms = millis();
    float v = analogRead(VOLTAGE_PIN) *  (5.0 / 1023.0);
    Serial.println(v);
  }

  if (irrecv.decode(&results)) {
    if (results.decode_type == NEC) {
      if(results.value != -1) {
        Serial.println(results.value,HEX);
        switch(results.value) {
          case 0xDC2300FF:
            if(STATE_POWER_OFF == projector.GetState())
              projector.Shutdown(false);
            else
              projector.PowerOn();
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
