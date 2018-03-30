# bluezTest Linux下的Bluez的应用

bluez是linux官方蓝牙协议栈。

- 组成
 - bluez分为两个部分：内核代码和用户态程序及工具集
 - 内核代码：bluez核心协议和驱动程序等模块组成
 - 用户态程序及工具集：应用程序接口和bluez工具集
 
## 编译bluez-5.49
需要先编译安装以下包：
- bluez-libs
- expat
- dbus
- glib
- bluez-utils
- libusb

在这里就不写了，网上教程很多。交叉编译后，把生成的/bin /sbin /lib下的so放到板子的lib目录。

## 测试
- hciconfig hci0 up  启用蓝牙
- hciconfig hci0 iscan配置开发板蓝牙可被查找
- hcitool scan 查找蓝牙
- Scanning ...
 - 04:02:1F:A2:B2:AF       xxx         
 - 00:13:EF:A0:00:AF       xxx         

## 应用编程
其实在linux上的bluetooth应用开发，都是基于内核提供了核心驱动和模块，我们移植bluez协议栈只是为了得到一些工具和API来驱动蓝牙设备而已。

当然了，在应用编程之前要先启动蓝牙服务，这个可以写个脚本来自动化方便开启，停止等动作的。后面会整理下，共享出来。

本项目基于bluez协议与epoll模型实现设备的蓝牙连接与数据交互功能。
___________________________________-
#### 更新
1.增加蓝牙启动脚本
