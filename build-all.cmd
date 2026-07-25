@echo off
set "Path="
set "PATH=C:\Windows\System32;C:\Windows"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
msbuild Raven.sln /p:Configuration=Release /p:Platform=x64 /m
