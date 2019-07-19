#pragma once

#define DEBUG_PRINT   1
enum{
  CXNProjector_CMD_OPEN           = 0x00, // 开机??       参数 00
  CXNProjector_CMD_GET_LIGHT      = 0x42, // 获取亮度
  CXNProjector_CMD_SET_LIGHT      = 0x43, // 设置亮度:    范围: -31 到 31
  CXNProjector_CMD_GET_CONTRAST   = 0x44, // 获取对比度
  CXNProjector_CMD_SET_CONTRAST   = 0x45, // 设置对比度:  范围: -15 到 15
  CXNProjector_CMD_SET_CLORMA     = 0x47, // 设置色度:    范围: -15 到 15
  CXNProjector_CMD_GET_SATURATION = 0x48, // 获取饱和度
  CXNProjector_CMD_SET_SATURATION = 0x49, // 设置饱和度:  范围: -15 到 15
  CXNProjector_CMD_GET_SHARP      = 0x4E, // 获取锐度:    寄存器位置 00
  CXNProjector_CMD_SET_SHARP      = 0X4F, // 设置锐度:    范围: 0   到 6
};

enum CXNProjector_State{
    STATE_POWER_OFF   = 0,  //光机断电状态.
    STATE_POWER_ON    = 1,  //光机开机状态.
    STATE_READY       = 2,  //光机引导完成.
    STATE_ACTIVE      = 3,  //光机开启了视频输入.
    STATE_MUTE        = 4,  //光机静音(黑屏)状态
    STATE_BOOT_READY_REBOOT = 0xFD, //
    STATE_BOOT_READY_OFF = 0xFE   //光机准备好了, 可以断开电源.
};

// 光机供电引脚接MOS管
#define CXNProjector_POWER_PIN  12

#pragma pack(1)
class CXNProjector {
public:
  CXNProjector();
  ~CXNProjector();
  //  打开光机供电, 
  //建议 用一个ADC引脚读取电压 稳定在4.5v以上后再开启供电.
  void PowerOn();
  //  关闭光机供电
  //提示 只有再光机准备好了可以关机的情况下才允许断开电源
  void PowerOff();

  // 视频信号输入启停
  bool StartInput();
  bool StopInput();

  // 关机指令
  bool Shutdown(bool isReboot = false);

  bool GetTrubleInfo();
  bool ClearTrubleInfo();
  void GetDefault();

  bool GetTemperature ();
  //读取光机的通知信息.
  int ReadNotify(uint8_t * data, int num);
  
  //获取所有图像质量信息
  bool GetAllPictureQualityInfo();
  
//  bool GetTemperature ();
  //  设置亮度
  bool SetLight(int8_t val);
  //  设置锐度
  bool SetSharp(int8_t val);
  // 设置对比度
  bool SetContrast(int8_t val);
  // 设置饱和度
  bool SetSaturat(int8_t val);

  // 图像翻转
  bool SetFlip(int8_t flip);
  // 左右梯形校正
  bool SetPan(int8_t flip);
  // 上下梯形校正
  bool SetTilt(int8_t flip);
  // 设置视频位置, 同时设置反转 梯形校正
  bool SetVideoPosition();
  // 处理CMD_REQ 通知
  void OnNotify();
  //特定状态通知处理
  void OnBootNotify(uint8_t * data, int num);
public:
  CXNProjector_State GetState() {return stat;};
private:
  CXNProjector_State stat;
  int8_t m_Contrast;    // 对比度   -15 ~ 15
  int8_t m_Brightness;  // 亮度     -31 ~ 31
  int8_t m_HueU;        // 色调U    -15 ~ 15
  int8_t m_HueV;        // 色调V    -15 ~ 15
  int8_t m_SaturationU; // 饱和度U   -15 ~ 15
  int8_t m_SaturationV; // 饱和度V   -15 ~ 15
  int8_t m_Sharpness;   // 锐度     0~6
  
  int8_t m_Pan;         // 左右梯形校正.     -30~30
  int8_t m_Tilt;        // 上下梯形校正.     -20~30
  int8_t m_Flip;        // 反转 0 不反转 1  左右, 2 上下, 3 左右+上下
};

#pragma pop() 
