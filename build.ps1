clear
Write-Host "Building driver..."

msbuild driver\driver.vcxproj `
    /p:Configuration=Release `
    /p:Platform=x64

Write-Host "Building user program..."

cl /EHsc user\user.cpp /Fe:user.exe

Write-Host "Build finished"