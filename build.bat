@echo off
cls

glslc.exe assets/PositionColor.vert -o assets/PositionColor.vert.spv
glslc.exe assets/PositionColor.frag -o assets/PositionColor.frag.spv
glslc.exe assets/screen.vert -o assets/screen.vert.spv
glslc.exe assets/screen.frag -o assets/screen.frag.spv
glslc.exe assets/depth_only.vert -o assets/depth_only.vert.spv
glslc.exe assets/depth_only.frag -o assets/depth_only.frag.spv
glslc.exe assets/circle.vert -o assets/circle.vert.spv
glslc.exe assets/circle.frag -o assets/circle.frag.spv
glslc.exe assets/triangle.vert -o assets/triangle.vert.spv
glslc.exe assets/mesh.vert -o assets/mesh.vert.spv

mkdir build 2> NUL

set warn=-W4 -wd 4505 -wd 4201 -D_CRT_SECURE_NO_WARNINGS

pushd build
	cl ..\src\main.cpp -nologo -std:c++20 -Zc:strictStrings- -Od -Zi %warn% -I..\src -link ..\SDL3.lib
popd

copy build\main.exe main.exe > NUL