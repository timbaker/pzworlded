#!/bin/bash
SRC=$(pwd)/..
BUILD=$(realpath $SRC/../Qt_6_11_1_for_macOS_Release/bin/)
DESTROOT=$(realpath $SRC/../../ProjectZomboid)
DEST=$DESTROOT/WorldEd

mkdir -p $DESTROOT
mkdir -p $DEST

rm -rf $DEST/PZWorldEd.app
cp -a $BUILD/PZWorldEd.app $DEST

# Add all necessary Qt libraries
~/Qt/6.11.1/macOS/bin/macdeployqt $DEST/PZWorldEd.app

# Remove all symlinks, they won't work on Steam
find $DEST/PZWorldEd.app/Contents -type l -delete

mv $DEST/PZWorldEd.app/Contents/Frameworks/libtiled.1.0.0.dylib $DEST/PZWorldEd.app/Contents/Frameworks/libtiled.1.dylib
mv $DEST/PZWorldEd.app/Contents/Frameworks/libzlib1.1.0.0.dylib $DEST/PZWorldEd.app/Contents/Frameworks/libzlib1.1.dylib

cp -a $SRC/LICENSE.BSD $DEST
cp -a $SRC/LICENSE.GPL $DEST
cp -a $SRC/LICENSE.QT6 $DEST

