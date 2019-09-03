call "D:\VisualStudioCommunity2013\VC\vcvarsall.bat" x86_amd64
mkdir C:\Programming\PZWorldEd\dist64
cd C:\Programming\PZWorldEd\dist64
"C:\Programming\QtSDK2015\5.7\msvc2013_64\bin\qmake.exe" C:\Programming\PZWorldEd\pzworlded\PZWorldEd.pro -r -spec win32-msvc2013 "CONFIG+=release"
"C:\Programming\QtSDK2015\Tools\QtCreator\bin\jom.exe"
"C:\Programming\TclTk\8.5.x\32bit-mingw\bin\tclsh85.exe" C:\Programming\PZWorldEd\pzworlded\dist.tcl 64bit
PAUSE
