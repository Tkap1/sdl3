@echo off
cls

glslc.exe assets/PositionColor.vert -o assets/PositionColor.vert.spv
glslc.exe assets/PositionColor.frag -o assets/PositionColor.frag.spv
glslc.exe assets/screen.vert -o assets/screen.vert.spv
glslc.exe assets/screen.frag -o assets/screen.frag.spv
glslc.exe assets/depth_only.vert -o assets/depth_only.vert.spv
glslc.exe assets/depth_only.frag -o assets/depth_only.frag.spv

mkdir build 2> NUL

pushd build
	cl ..\src\main.cpp -nologo -std:c++20 -Zc:strictStrings- -Od -Zi -W4 -wd 4505 -I..\src -link ..\SDL3.lib
popd

copy build\main.exe main.exe > NUL