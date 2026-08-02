SRC=$(pwd)/..
BUILD=$SRC/../build-worlded-docker
APPIMAGE=$SRC/../linuxdeploy/PZWorldEd-x86_64.AppImage
DESTROOT=$SRC/../dist-tiled-docker
DEST=$DESTROOT/WorldEd

mkdir -p $DESTROOT
mkdir -p $DEST
cp -a $APPIMAGE $DEST

cp -a $SRC/Blends.txt $DESTROOT/TileZed/share/tilezed/config
cp -a $SRC/Rules.txt $DESTROOT/TileZed/share/tilezed/config
cp -a $SRC/WorldDefaults.txt $DESTROOT/TileZed/share/tilezed/config

cp -a $SRC/dist/PZWorldEd-x86_64.AppImage.sh $DEST
chmod +x $DEST/PZWorldEd-x86_64.AppImage.sh

cp -a $SRC/LICENSE.BSD $DEST
cp -a $SRC/LICENSE.GPL $DEST
cp -a $SRC/LICENSE.QT6 $DEST

