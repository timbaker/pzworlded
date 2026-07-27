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
    python3 \
    python3-pip \
    python3-venv \
    libgl1-mesa-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libxkbcommon-x11-0 \
    libxcb-xinerama0 \
    libxcb-cursor0 \
    && rm -rf /var/lib/apt/lists/*

# Create a virtual environment and install aqtinstall
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
RUN pip install aqtinstall

# Install Qt (Arguments: aqt install-qt <host> <target> <version>)
# Example: Linux, Desktop target, Qt 6.8.0
RUN aqt install-qt linux desktop 5.15.2 gcc_64 --outputdir /opt/qt

# Set environment variables so CMake and the system can find Qt
ENV QT_DIR=/opt/qt/5.15.2/gcc_64
ENV CMAKE_PREFIX_PATH=${QT_DIR}/lib/cmake:${CMAKE_PREFIX_PATH}
ENV PATH=${QT_DIR}/bin:$PATH

# Set the workspace inside the container
WORKDIR /workspace
