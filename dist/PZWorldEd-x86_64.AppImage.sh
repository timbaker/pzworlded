#!/bin/sh
appname=`basename $0 | sed s,\.sh$,,`

dirname=`dirname $0`
tmp="${dirname#?}"

if [ "${dirname%$tmp}" != "/" ]; then
dirname=$PWD/$dirname
fi
export LD_LIBRARY_PATH=$dirname/lib
export QT_QPA_PLATFORM="xcb;wayland"
export QT_WAYLAND_DISABLE_WINDOWDECORATION=0
$dirname/$appname "$@"

