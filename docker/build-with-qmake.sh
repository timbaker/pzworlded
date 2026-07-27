sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
    bash -c "mkdir -p build-worlded-docker && cd build-worlded-docker && qmake ../pzworlded/PZWorldEd.pro INSTALL_ONLY_BUILD=1 && make -j$(nproc) && make install"

