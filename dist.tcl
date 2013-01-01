console show

set SOURCE1 C:/Programming/Tiled/PZWorldEd/build-msvc-release
set SOURCE2 C:/Programming/Tiled/PZWorldEd/PZWorldEd
set QT_BINARY_DIR C:/Programming/Qt/qt-build/bin
set QT_PLUGINS_DIR C:/Programming/Qt/qt-build/plugins
set DEST C:/Programming/Tiled/PZWorldEd/PZWorldEd-Win32

proc copyFile {SOURCE DEST name} {
    set src [file join $SOURCE $name]
    set dst [file join $DEST $name]
    if {![file exists $src]} {
	error "no such file \"$src\""
    }
    if {![file exists $dst] || ([file mtime $src] > [file mtime $dst])} {
	file mkdir [file dirname $dst]
	file copy -force $src $dst
	puts "copied $name"
    } else {
	puts "skipped $name"
    }
    return
}

copyFile $SOURCE1 $DEST PZWorldEd.exe
copyFile $SOURCE1 $DEST tiled.dll

copyFile $SOURCE2 $DEST Blends.txt
copyFile $SOURCE2 $DEST MapBaseXML.txt
copyFile $SOURCE2 $DEST Roads.txt
copyFile $SOURCE2 $DEST Rules.txt
copyFile $SOURCE2 $DEST qt.conf

copyFile $QT_BINARY_DIR $DEST QtCore4.dll
copyFile $QT_BINARY_DIR $DEST QtGui4.dll
copyFile $QT_BINARY_DIR $DEST QtOpenGL4.dll
copyFile $QT_BINARY_DIR $DEST QtXml4.dll

copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qgif4.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qjpeg4.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qtiff4.dll

copyFile $QT_PLUGINS_DIR $DEST/plugins codecs/qcncodecs4.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins codecs/qjpcodecs4.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins codecs/qkrcodecs4.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins codecs/qtwcodecs4.dll

