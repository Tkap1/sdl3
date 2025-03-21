@echo off
cls

glslc.exe assets/PositionColor.vert -o assets/PositionColor.vert.spv
glslc.exe assets/PositionColor.frag -o assets/PositionColor.frag.spv

pushd build
	cl ..\src\main.cpp -nologo -std:c++20 -Zc:strictStrings- -Od -Zi -W4 -wd 4505 -I..\src -I..\..\my_libs2 -link ..\SDL3.lib
popd

copy build\main.exe main.exe > NUL