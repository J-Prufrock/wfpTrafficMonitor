$Driver="WfpMonitor"
$DriverPath="$PWD\driver\driver.sys"

sc.exe stop $Driver
sc.exe delete $Driver

sc.exe create $Driver type= kernel binPath= "$DriverPath"

sc.exe start $Driver

.\user.exe

sc.exe stop $Driver
sc.exe delete $Driver
