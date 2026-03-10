# WFP Traffic Monitor

## Environment
Windows 11
Visual Studio 2022
Windows SDK 10 (10.0.26100.7705)
WDK
Windows调试器 WinDbg

## Build 

使用VS2022打开解决方案并生成,在.\x64\Release目录下即可找到生成的exe文件，sys文件则在.\x64\Release\driver目录下。

## run
1. 打开winodws(建议使用虚拟机)，管理员权限运行以下命令启用测试签名模式和允许运行脚本文件，重启后才能加载未签名的驱动。
```
bcdedit /set testsigning on 
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
```
2. 复制项目中.\x64\Release目录并以管理员权限在终端打开，里面有run.ps1可以执行自动化运行。
3. 在run.ps1中，会先停止和卸载驱动，再执行user程序写入result.txt，最后会再次停止和卸载驱动。
4. 另外clean.ps1可以专门停止和卸载驱动。
5. 生成的result.txt在该目录下，里面记录了流量监控的结果。