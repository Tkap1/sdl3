@echo off
cls

if defined VSCMD_ARG_TGT_ARCH (
	goto compile
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
	call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
	goto compile
)
if exist "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
	call "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
	goto compile
)

:compile

if not defined VSCMD_ARG_TGT_ARCH (
	echo cl.exe not found!
	goto end
)

mkdir build 2> NUL

set warn=-W4 -wd 4505 -wd 4201 -D_CRT_SECURE_NO_WARNINGS

pushd build
	cl ..\src\main.cpp -nologo -std:c++20 -Zc:strictStrings- -Od -Zi %warn% -I..\src -link ..\SDL3.lib ..\shaderc_shared.lib
popd

copy build\main.exe main.exe > NUL

:end