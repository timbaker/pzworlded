sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
    bash -c "cd pzworlded/dist && bash make-dist-linux64.sh"

#sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
#    bash -c "ls \$(qtpaths --plugin-directory)"

#sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
#    bash -c "ls /opt/qt/5.15.2/gcc_64/lib"

