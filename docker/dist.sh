sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
    bash -c "cd pzworlded/dist && bash linuxdeploy.sh"

sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
    bash -c "cd pzworlded/dist && bash make-dist-linux64.sh"

sudo chown -R $USER:$USER ../../dist-tiled-docker

#sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
#    bash -c "ls \$(qtpaths --plugin-directory)"

#sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
#    bash -c "ls /opt/qt/6.11.1/gcc_64/lib"

