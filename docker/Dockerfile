# Use a stable Ubuntu LTS version
FROM ubuntu:22.04

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools, dependencies, and Python (required for aqt)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    make \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    libdbus-1-dev \
    libgl1-mesa-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libxkbcommon-x11-0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-xkb1 \
    libxkbcommon-x11-0 \
    libxcb-xinerama0 \
    libxcb-cursor0 \
    libxcb-shape0 \
    libxcb-shm0 \
    wget \
    ca-certificates \
    libfuse2 \
    file \
    && rm -rf /var/lib/apt/lists/*

# Create a virtual environment and install aqtinstall
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
RUN pip install aqtinstall

# Install Qt (Arguments: aqt install-qt <host> <target> <version>)
# See https://ddalcino.github.io/aqt-list-server/
RUN aqt install-qt linux desktop 6.11.1 linux_gcc_64 -m qt3d qt5compat qtwaylandcompositor --outputdir /opt/qt

# Set environment variables so CMake and the system can find Qt
ENV QT_DIR=/opt/qt/6.11.1/gcc_64
ENV CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake:${CMAKE_PREFIX_PATH}
ENV PATH=${QT_DIR}/bin:$PATH

# Download linuxdeploy and the qt plugin into a global path
RUN wget -P /usr/local/bin https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
 && wget -P /usr/local/bin https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
 && chmod +x /usr/local/bin/linuxdeploy-x86_64.AppImage \
 && chmod +x /usr/local/bin/linuxdeploy-plugin-qt-x86_64.AppImage

# Create symlinks so you can run them using shorter commands (optional but recommended)
RUN ln -s /usr/local/bin/linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy
RUN ln -s /usr/local/bin/linuxdeploy-plugin-qt-x86_64.AppImage /usr/local/bin/linuxdeploy-plugin-qt

ENV APPIMAGE_EXTRACT_AND_RUN=1

# Set the workspace inside the container
WORKDIR /workspace
