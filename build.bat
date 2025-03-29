@echo off
cls

mkdir build 2> NUL

set warn=-W4 -wd 4505 -wd 4201 -D_CRT_SECURE_NO_WARNINGS

pushd build
	cl ..\src\main.cpp -nologo -std:c++20 -Zc:strictStrings- -Od -Zi %warn% -I..\src -link ..\SDL3.lib ..\shaderc_shared.lib
popd

copy build\main.exe main.exe > NUL