$Driver="WfpMonitor"
$DriverPath="$PWD\x64\Release\driver\driver.sys"

sc.exe stop $Driver
sc.exe delete $Driver

sc.exe create $Driver type= kernel binPath= "$DriverPath"

sc.exe start $Driver

.\x64\Release\user.exe

sc.exe stop $Driver
sc.exe delete $Driver

Get-Content result.txt