QTDIR=~/Qt/5.8/gcc_64
SRC=~/Programming/TileZed/pzworlded
BUILD=~/Programming/TileZed/build-PZWorldEd-Desktop_Qt_5_8_0_GCC_64bit-Release
DEST=~/Programming/TileZed/WorldEd

mkdir $DEST
cp -avr $BUILD/bin/ $DEST
#cp -ar $BUILD/share/ $DEST

mkdir $DEST/lib
cp -a $BUILD/lib/libtiled.so.1.0.0 $DEST/lib/libtiled.so.1
cp -a $BUILD/lib/libzlib1.so.1.0.0 $DEST/lib/libzlib1.so.1
#for file in $BUILD/lib/libtiled.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $BUILD/lib/libzlib1.so*; do cp -a "$file" "$DEST/lib/"; done

cp -a $SRC/Blends.txt $DEST
cp -a $SRC/Rules.txt $DEST
cp -a $SRC/WorldDefaults.txt $DEST

cp -a $SRC/dist/qt.conf.linux $DEST/bin/qt.conf
cp -a $SRC/dist/PZWorldEd.sh $DEST
chmod +x $DEST/PZWorldEd.sh

cp $QTDIR/lib/libQt5Core.so.5.8.0 $DEST/lib/libQt5Core.so.5
cp $QTDIR/lib/libQt5DBus.so.5.8.0 $DEST/lib/libQt5DBus.so.5
cp $QTDIR/lib/libQt5Gui.so.5.8.0 $DEST/lib/libQt5Gui.so.5
cp $QTDIR/lib/libQt5Network.so.5.8.0 $DEST/lib/libQt5Network.so.5
cp $QTDIR/lib/libQt5OpenGL.so.5.8.0 $DEST/lib/libQt5OpenGL.so.5
cp $QTDIR/lib/libQt5Widgets.so.5.8.0 $DEST/lib/libQt5Widgets.so.5
cp $QTDIR/lib/libicudata.so.56.1 $DEST/lib/libicudata.so.56
cp $QTDIR/lib/libicui18n.so.56.1 $DEST/lib/libicui18n.so.56
cp $QTDIR/lib/libicuuc.so.56.1 $DEST/lib/libicuuc.so.56
cp $QTDIR/lib/libQt5XcbQpa.so.5.8.0 $DEST/lib/libQt5XcbQpa.so.5

#for file in $QTDIR/lib/libQt5Gui.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5Network.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5OpenGL.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5Widgets.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libicu*.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5DBus.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5XcbQpa.so*; do cp -a "$file" "$DEST/lib/"; done

mkdir $DEST/plugins
mkdir $DEST/plugins/imageformats
cp -a $QTDIR/plugins/imageformats/libqgif.so $DEST/plugins/imageformats/
cp -a $QTDIR/plugins/imageformats/libqjpeg.so $DEST/plugins/imageformats/
mkdir $DEST/plugins/platforms
cp -a $QTDIR/plugins/platforms/libqxcb.so $DEST/plugins/platforms/
