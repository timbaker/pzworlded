SRC=$(pwd)/..
BUILD=$SRC/../Qt_6_11_1_for_macOS-Release/bin/
DESTROOT=$SRC/../../ProjectZomboid
DEST=$DESTROOT/WorldEd

mkdir $DESTROOT
mkdir $DEST

cp -ra $BUILD/PZWorldEd.app $DEST

cp -a $SRC/LICENSE.BSD.txt $DEST
cp -a $SRC/LICENSE.GPL.txt $DEST
cp -a $SRC/LICENSE.QT6 $DEST

