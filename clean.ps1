$Driver="WfpMonitor"
$DriverPath="$PWD\x64\Release\driver\driver.sys"

sc.exe stop $Driver
sc.exe delete $Driver
