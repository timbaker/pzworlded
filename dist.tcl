if {[llength [info commands console]]} {
    console show
    update
}

set BIN C:/Programming/Tiled/PZWorldEd/build-PZWorldEd-Qt4_MSVC_64-Release
set SRC C:/Programming/Tiled/PZWorldEd/PZWorldEd
set QT_BINARY_DIR C:/Programming/Qt/qt-4.8.x-MSVC-2012-64bit-build/bin
set QT_PLUGINS_DIR C:/Programming/Qt/qt-4.8.x-MSVC-2012-64bit-build/plugins
set DEST {C:\Users\Tim\Desktop\ProjectZomboid\Tools\TileZed\WorldEd}

if {$argc > 0} {
    switch -- [lindex $argv 0] {
        32bit {
            puts "dist.tcl: 32-bit"
            set BIN C:/Programming/Tiled/PZWorldEd/build-msvc-release
            set QT_BINARY_DIR C:/Programming/Qt/qt-build/bin
            set QT_PLUGINS_DIR C:/Programming/Qt/qt-build/plugins
            set DEST {C:\Users\Tim\Desktop\ProjectZomboid\Tools\TileZed\WorldEd}
        }
        64bit {
            puts "dist.tcl: 64-bit"
        }
        default {
            error "unknown arguments to dist.tcl: $argv"
        }
    }
}

proc copyFile {SOURCE DEST name {name2 ""}} {
    if {$name2 == ""} { set name2 $name }
    set src [file join $SOURCE $name]
    set dst [file join $DEST $name2]
    if {![file exists $src]} {
        error "no such file \"$src\""
    }
    set relative $name
    foreach var {BIN SRC QT_BINARY_DIR QT_PLUGINS_DIR} {
        if {[string match [set ::$var]* $src]} {
            set relative [string range $src [string length [set ::$var]] end]
        }
    }
    if {![file exists $dst] || ([file mtime $src] > [file mtime $dst]) || ([file size $src] != [file size $dst])} {
        file mkdir [file dirname $dst]
        if {[file extension $name2] == ".txt"} {
            set chan [open $src r]
            set text [read $chan]
            close $chan
            set chan [open $dst w]
            fconfigure $chan -translation crlf
            puts -nonewline $chan $text
            close $chan
            puts "copied $relative (crlf)"
        } else {
            file copy -force $src $dst
            puts "copied $relative"
        }
    } else {
        puts "skipped $relative"
    }
    return
}

#copyFile {C:\Programming\Tiled} $DEST vcredist_x86.exe

copyFile $BIN $DEST PZWorldEd.exe
copyFile $BIN $DEST tiled.dll
copyFile $BIN $DEST zlib1.dll

copyFile $SRC $DEST Blends.txt
copyFile $SRC $DEST MapBaseXML.txt
copyFile $SRC $DEST Roads.txt
copyFile $SRC $DEST Rules.txt
copyFile $SRC $DEST qt.conf

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

proc removeFD {dir name} {
    foreach f [glob -nocomplain -types {d f} -dir $dir $name] {
        puts "removing $f"
        file delete -force $f
    }
    foreach f [glob -nocomplain -types d -dir $dir *] {
        if {$f == "." || $f == ".."} continue
        removeFD $f $name
    }
    return
}
if 1 {
removeFD {C:\Users\Tim\Desktop\ProjectZomboid\Tools} vcredist*
removeFD {C:\Users\Tim\Desktop\ProjectZomboid\Tools} .pzeditor
removeFD {C:\Users\Tim\Desktop\ProjectZomboid\Tools} lots
removeFD {C:\Users\Tim\Desktop\ProjectZomboid\Tools} *.bak
removeFD {C:\Users\Tim\Desktop\ProjectZomboid\Tools} EnableDeveloperFeatures.txt
}
