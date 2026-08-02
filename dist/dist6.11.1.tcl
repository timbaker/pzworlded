if {[llength [info commands console]]} {
    console show
    update
}

set BIN C:/Programming/PZWorldEd/dist6.11.1
set SRC C:/Programming/PZWorldEd/pzworlded
set QT_DIR C:/Programming/QtSDK2015/6.11.1/msvc2022_64
set DEST {C:\Programming\ProjectZomboid\Tools\windows\WorldEd}
# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\v143
set REDIST vc_redist.x64.2015-2022.exe

if {$argc > 0} {
    switch -- [lindex $argv 0] {
        64bit {
            puts "dist.tcl: 64-bit"
        }
        default {
            error "unknown arguments to dist.tcl: $argv"
        }
    }
}

set QT_BINARY_DIR $QT_DIR/bin
set QT_PLUGINS_DIR $QT_DIR/plugins

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

copyFile {C:\Programming\TileZed} $DEST $REDIST vc_redist.x64.exe
copyFile $BIN $DEST PZWorldEd.exe
copyFile $BIN $DEST tiled.dll
copyFile $BIN $DEST zlib1.dll

copyFile $SRC $DEST LICENSE.GPL LICENSE.BSD.txt
copyFile $SRC $DEST LICENSE.GPL LICENSE.GPL.txt
copyFile $SRC $DEST LICENSE.QT6 LICENSE.QT6.txt

copyFile $SRC $DEST Blends.txt
copyFile $SRC $DEST MapBaseXML.txt
copyFile $SRC $DEST MapToPNG.txt
copyFile $SRC $DEST Roads.txt
copyFile $SRC $DEST Rules.txt
copyFile $SRC $DEST WorldDefaults.txt
copyFile $SRC $DEST qt.conf

copyFile $QT_BINARY_DIR $DEST Qt6Core.dll
copyFile $QT_BINARY_DIR $DEST Qt6Core5Compat.dll
copyFile $QT_BINARY_DIR $DEST Qt6Gui.dll
copyFile $QT_BINARY_DIR $DEST Qt6Network.dll
copyFile $QT_BINARY_DIR $DEST Qt6OpenGL.dll
copyFile $QT_BINARY_DIR $DEST Qt6OpenGLWidgets.dll
copyFile $QT_BINARY_DIR $DEST Qt6Svg.dll
copyFile $QT_BINARY_DIR $DEST Qt6Widgets.dll
copyFile $QT_BINARY_DIR $DEST Qt6Xml.dll

copyFile $QT_PLUGINS_DIR $DEST/plugins iconengines/qsvgicon.dll

copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qgif.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qjpeg.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qsvg.dll
copyFile $QT_PLUGINS_DIR $DEST/plugins imageformats/qtiff.dll

copyFile $QT_PLUGINS_DIR $DEST/plugins platforms/qwindows.dll

copyFile $QT_PLUGINS_DIR $DEST/plugins styles/qmodernwindowsstyle.dll

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
removeFD {C:\Programming\PZWorldEd} vcredist*
removeFD {C:\Programming\PZWorldEd} .pzeditor
removeFD {C:\Programming\PZWorldEd} lots
removeFD {C:\Programming\PZWorldEd} *.bak
removeFD {C:\Programming\PZWorldEd} EnableDeveloperFeatures.txt
removeFD {C:\Programming\PZWorldEd} Qt*4.dll
}
