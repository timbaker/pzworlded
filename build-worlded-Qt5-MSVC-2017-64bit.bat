call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
mkdir C:\Programming\PZWorldEd\dist64
cd C:\Programming\PZWorldEd\dist64
"C:\Programming\QtSDK2015\5.9.3\msvc2017_64\bin\qmake.exe" C:\Programming\PZWorldEd\pzworlded\PZWorldEd.pro -r -spec win32-msvc "CONFIG+=release"
"C:\Programming\QtSDK2015\Tools\QtCreator\bin\jom.exe"
"C:\Programming\TclTk\8.5.x\32bit-mingw\bin\tclsh85.exe" C:\Programming\PZWorldEd\pzworlded\dist.tcl 64bit
PAUSE
