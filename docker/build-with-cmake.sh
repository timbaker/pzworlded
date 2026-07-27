sudo docker run --rm -v "$(pwd)":/workspace qt-builder \
    bash -c "cmake -B build -G Ninja && cmake --build build"
