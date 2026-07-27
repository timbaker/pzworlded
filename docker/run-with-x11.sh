# Allow local container connections to your X server
xhost +local:docker

# Run the binary passing through the display variable and IPC socket
sudo docker run --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$(pwd)":/workspace \
    qt-builder ./build/MyQtApp

# Revoke permission when done to keep your host secure
xhost -local:docker
