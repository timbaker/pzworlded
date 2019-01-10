call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64

REM This is where the Visual Studio project files will be created.
mkdir ..\build-mapbox-64
cd ..\build-mapbox-64

REM Generate the project files.
REM Mapbox requires clang-cl.exe which is part of LLVM 7.0+, it won't build with Microsoft's cl.exe
REM "-T LLVM" should be enough but CMAKE_CXX_COMPILER and CMAKE_C_COMPILER are set to cl.exe for some reason.
SET LLVMDIR=D:\LLVM
SET CURRENTDIR="%cd%"
"C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 15 2017 Win64" -T LLVM ^
    -DCMAKE_CXX_COMPILER=%LLVMDIR%\bin\clang-cl.exe -DCMAKE_C_COMPILER=%LLVMDIR%\bin\clang-cl.exe ^
    -DCMAKE_INSTALL_PREFIX=%CURRENTDIR%\..\install-mapbox-64 ^
    -DMBGL_PLATFORM=qt -DWITH_NODEJS=OFF -DWITH_ERROR=OFF ^
    -DWITH_QT_DECODERS=ON -DWITH_QT_I18N=ON ^
    -DQt5Core_DIR=C:\Programming\QtSDK2015\5.9.3\msvc2017_64\lib\cmake\Qt5Core ^
    -DQt5Gui_DIR=C:\Programming\QtSDK2015\5.9.3\msvc2017_64\lib\cmake\Qt5Gui ^
    -DQt5Network_DIR=C:\Programming\QtSDK2015\5.9.3\msvc2017_64\lib\cmake\Qt5Network ^
    -DQt5OpenGL_DIR=C:\Programming\QtSDK2015\5.9.3\msvc2017_64\lib\cmake\Qt5OpenGL ^
    -DQt5Sql_DIR=C:\Programming\QtSDK2015\5.9.3\msvc2017_64\lib\cmake\Qt5Sql ^
    D:\pz\worktree\build40-weather\mapbox-gl-native

PAUSE

