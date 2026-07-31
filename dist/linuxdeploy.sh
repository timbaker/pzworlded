mkdir ../../linuxdeploy
cd ../../linuxdeploy

# 1. Force the inclusion of the Wayland platform libraries
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"

# 2. Force the inclusion of the core Wayland protocol engine
export EXTRA_QT_MODULES="waylandcompositor"

# 3. Force the shell and graphics routing sub-folders
export EXTRA_QT_PLUGINS="wayland-shell-integration;wayland-graphics-integration-client"

rm -rf ./AppDir

linuxdeploy \
  --appdir AppDir \
  --desktop-file ../pzworlded/PZWorldEd.desktop \
  --icon-file ../pzworlded/src/editor/images/worlded-icon-32.png \
  --executable ../build-worlded-docker/bin/PZWorldEd \
  --plugin qt \
  --output appimage

#unset LD_LIBRARY_PATH
#unset QT_PLUGIN_PATH
#QT_QPA_PLATFORM=wayland ./PZWorldEd-x86_64.AppImage
