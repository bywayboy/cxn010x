## Sony CXN010x 光机项目

本项目基于 Arduino Nano v3 开发.


模块引脚定义如下:

| 引脚 | 引脚编号 |  定义 | 说明 |
|:--:|:--:|:--|:--|
| A0 | 14 | 电压测量 | 测量光机电压, 低于4.7 不响应开机指令. |
| A3 | 17 | 开关 | 投影光机电源开关(模值控制) |
| A4 | 18 | SDA | 接光机I2C SDA 引脚 |
| A5 | 19 | SCL | 接光机I2C SCL 引脚 |
| D2 | 2  | CMD_REQ | 接光机 CMD_REQ 引脚(用于判断光机通知信息.) |
| D6 | 6  | IR | 接行外遥控接受头的 DATA 引脚. (用于接收红外遥控器指令.) |


### 注意事项

光机所有IO口都是 1.8v,控制板进行了一次电平转换 3.3v，然而 Arduino Nano 工作在5v 电压下。需要对连接光机的所有IO口的电平转换到 3.3v.

光机通过 A3 引脚 连接到 MOSFET 控制光机电源开关状态.



## 赞助

<img src="https://github.com/bywayboy/cxn010x/raw/master/docs/alipay.jpg" width="300" >