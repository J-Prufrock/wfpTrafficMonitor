$Driver="WfpMonitor"
$DriverPath="$PWD\driver\driver.sys"

sc.exe stop $Driver
sc.exe delete $Driver
