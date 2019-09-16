#include <IRremote.h>
//#include <avr/sleep.h>
#include <Wire.h>
#include <time.h>
#include <EEPROM.h>
#include "cxn010x.h"
#include "HexDump.h"

/*
 * 主板: Arduino Nano
 * 引导: Atmega328P(Old bootloader)
 *  引脚定义表:
      IIC1 PA4 SDA    PA5 SCL
*/






#define REMOUTE_RC_100  1
#define REMOUTE_DEFAULT 2
#define REMOUTE_COOCAA  3 //酷开遥控器

//#define REMOUTE_MODEL    REMOUTE_DEFAULT
#define REMOUTE_MODEL     REMOUTE_COOCAA

#if REMOUTE_MODEL == REMOUTE_RC_100
  #define RM_DECODE_TYPE  NEC
  #define RM_POWER_BTN    0xDC2300FF
  #define RM_RIGHT_BTN    0xDC2348B7
  #define RM_LEFT_BTN     0xDC2308F7
  #define RM_UP_BTN       0xDC23B04F
  #define RM_DOWN_BTN     0xDC23A857
  #define RM_OK_BTN       0xDC238877
  #define RM_VOL_UP_BTN   0xDC2330CF    //对应音量减
  #define RM_VOL_DOWN_BTN 0xDC23708F    //对应音量加
  #define RM_VOL_DEF_BTN  0x08F78A75    //对应静音键  恢复亮度默认
  #define RM_LIGHT_DEF_BTN 0xDC236897   //对应静音键
  #define RM_SAVE_BTN     0xDC238A75    //对应上下文(菜单)
  #define RM_DEF_BTN      0xDC230AF5    //对应返回键   恢复图像默认设置
#elif REMOUTE_MODEL == REMOUTE_DEFAULT
  #define RM_DECODE_TYPE  NEC
  #define RM_POWER_BTN    0x08F7BA45
  #define RM_RIGHT_BTN    0X08F7807F
  #define RM_LEFT_BTN     0x08F7E01F
  #define RM_UP_BTN       0x08F7A25D
  #define RM_DOWN_BTN     0x08F7AA55
  #define RM_OK_BTN       0x08F7827D
  #define RM_VOL_UP_BTN   0x08F7E817
  #define RM_VOL_DOWN_BTN 0x08F7E837
  #define RM_VOL_DEF_BTN  0x08F78A75    //对应 3D键  恢复亮度默认
  #define RM_SAVE_BTN     0x08F75AA5    //对应鼠标键  保存配置
  #define RM_DEF_BTN      0x08F700FF    //对应返回键   恢复图像默认设置
  #define RM_MUTE_BTN     0x08F7EF10    //关闭屏幕输出
  #define RM_PAUSE_BTN    0x08F79A65    //暂停播放按键
#elif REMOUTE_MODEL == REMOUTE_COOCAA
  #define RM_DECODE_TYPE  SAMSUNG
  #define RM_POWER_BTN    0x707030CF
  #define RM_RIGHT_BTN    0x7070A25D
  #define RM_LEFT_BTN     0x707022DD
  #define RM_UP_BTN       0x707042BD
  #define RM_DOWN_BTN     0x7070C23D
  #define RM_OK_BTN       0x7070629D
  #define RM_SAVE_BTN     0x70708877
  #define RM_DEF_BTN      0x7070DA25
  #define RM_VOL_UP_BTN   0x707028D7    //对应音量减
  #define RM_VOL_DOWN_BTN 0x7070A857    //对应音量加
  #define RM_VOL_DEF_BTN  0x00000000    //不存在该按键

  #define RM_PAUSE_BTN    0x70701EE1    //对应Home 按键 定时关机 按一次 定时30分钟

  #define RM_MUTE_BTN     0x7070B04F    //静音按键 关闭屏幕
  #define RM_LONG_HOME    0x70700AF5    //长按Home键, 进入相位校准
  #define RM_LONG_MENU    0x7070A15E    //长按菜单键 (进入光轴校准)  1,2 红纵横, 3,4 绿纵横, 5,6 蓝纵横
  #define RM_LONG_RETURN  0X7070D827    //长按返回键 (退出光轴/相位校准)
#endif

#define LED               13  // LED 引脚 PD13
#define CMD_REQ_PIN       2  // PD2 INT0
#define VOLTAGE_PIN       14  // PA0 (电压测量引脚)
#define RECV_PIN          PD6 // 红外遥控引脚

// 注意: 光机控制引脚改用模拟引脚控制. 引脚序号 17  对应主板上的 A3 引脚

IRrecv irrecv(RECV_PIN);
decode_results results;
CXNProjector projector;

unsigned long g_offtime ;

void setup() {

  pinMode(LED, OUTPUT);
  pinMode(CXNProjector_POWER_PIN, OUTPUT);
  pinMode(CMD_REQ_PIN, INPUT);
  pinMode(RECV_PIN, INPUT);
  irrecv.blink13(false);
  irrecv.enableIRIn();
  
  Serial.begin(115200);
  Wire.begin(); //初始化I2C
  analogWrite(CXNProjector_POWER_PIN, 0);
  g_offtime = 0;
  delay(200);
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


CXNProjector_State gstat;
void loop() {
  //这里暂时不使用中断控制
  if(projector.GetState() != STATE_POWER_OFF) {
    if(digitalRead(2)==HIGH) {
      delay(10);  //等待 10ms; 如果电平还是高电平,读取通知.
      // 循环读取,直到CMD_REQ 电平拉低了
      while(HIGH == digitalRead(CMD_REQ_PIN)) {
        Serial.println("CMD_REQ");
        projector.OnNotify();
      }
    }
  }

  if (irrecv.decode(&results)) {
    if (results.decode_type == RM_DECODE_TYPE) {
      if(results.value != -1) {
        Serial.println(results.value,HEX);
        switch(results.value) {
          case RM_POWER_BTN:
            Serial.println(projector.GetState(),HEX);
            if(STATE_POWER_OFF == projector.GetState()) {
              float v = analogRead(VOLTAGE_PIN) *  (5.0 / 1023.0);
              if(v >= 4.8){
                g_offtime = 0;
                projector.PowerOn();
                delay(50);
              }else{
                  Serial.println("ERR: LOW POWER!!");
              }
            }else{
              projector.Shutdown(false);
              Serial.println("INF: POWER OFF!!");
            }
            break;
        }

        gstat = projector.GetState();
        //关机状态 下列遥控指令不响应!
        if(STATE_POWER_OFF != gstat) {
          switch(results.value) {
            case RM_RIGHT_BTN: // 右
              projector.SetPan(+1);
              break;
            case RM_LEFT_BTN: // 左
              projector.SetPan(-1);
              break;
            case RM_UP_BTN: // 上
              projector.SetTilt(1);
              break;
            case RM_DOWN_BTN: // 下
              projector.SetTilt(-1);
              break;
            case RM_OK_BTN: // OK
              projector.SetFlip();
              break;
            case RM_VOL_UP_BTN: // VOL+ 亮度+
              switch(gstat){
              case STATE_OPTICAL:
                projector.OpticalAxisPlus();
                break;
              case STATE_BIPHASE:
                projector.BiphasePlus();
                break;
              default:
                projector.SetLight(+1);
                break;
              }
              break;
            case RM_VOL_DOWN_BTN: // VOL- 亮度-
              switch(gstat){
              case STATE_OPTICAL:
                projector.OpticalAxisMinus();
                break;
              case STATE_BIPHASE:
                projector.BiphaseMinus();
                break;
              default:
                projector.SetLight(-1);
                break;
              }
              break;
            case RM_VOL_DEF_BTN://
              projector.m_Brightness = 0;
              projector.SetLight(0);
              break;
            case RM_SAVE_BTN: // 上下文(保存)
              projector.SaveConfig();
              break;
            case RM_DEF_BTN: //返回 恢复所有位置信息
              projector.m_Pan = projector.m_Tilt = projector.m_Flip = 0;
              projector.SetVideoPosition();
              break;
#if REMOUTE_MODEL != REMOUTE_RC_100
            case RM_MUTE_BTN:
              projector.Mute();
              break;
            case RM_PAUSE_BTN:
              if(0 == g_offtime){
                g_offtime = millis() + (60000 * 30);  //延时半小时
                delay(60);
              }else{
                g_offtime +=(60000 * 30);  //延时半小时
                delay(60);
              }
              break;
#endif
#if REMOUTE_MODEL == REMOUTE_COOCAA
          case RM_LONG_HOME:      //长按Home键, 进入相位校准
            projector.EasyOpticalAxisSet();
            break;
          case RM_LONG_MENU:    //长按菜单键 (进入光轴校准)  1,2 红纵横, 3,4 绿纵横, 5,6 蓝纵横
            projector.EasyBiphaseSet();
            break;
          case RM_LONG_RETURN:   //长按返回键 (退出光轴/相位校准)
            if(gstat ==STATE_OPTICAL)
              projector.EasyOpticalAxisExit(1);
            else if(gstat == STATE_BIPHASE)
              projector.EasyBiphaseExit(1);
            break;
#endif
          }
        } // if
      }
    }else{
      
      Serial.print(results.decode_type,HEX);
      Serial.print("-");
      Serial.println(results.value,HEX);
    }
    irrecv.resume(); // Receive the next value
  }
  if(STATE_POWER_OFF != projector.GetState()) {
    if(g_offtime > 0) {
      if(millis() > g_offtime) {
        g_offtime = 0;
        projector.Shutdown(false);
      }
    }
  }
  
}
