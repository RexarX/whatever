# Dockerfile for Client
# Based on Ubuntu 24.04 LTS (Noble Numbat)
FROM ubuntu:24.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Set timezone
ENV TZ=UTC

# Install basic dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    curl \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    ca-certificates \
    gnupg \
    lsb-release \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# Install boost and dependencies
RUN apt-get update && apt-get install -y \
    libdevel-backtrace-perl \
    libboost-dev \
    && rm -rf /var/lib/apt/lists/*

# Install spdlog and dependencies
RUN apt-get update && apt-get install -y \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*

# Install OpenCV4 and dependencies
RUN apt-get update && apt-get install -y \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

# Install argparse
RUN apt-get update && apt-get install -y \
    libargparse-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Qt6 and development tools
RUN apt-get update && apt-get install -y \
    qt6-base-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    libqt6core6 \
    libqt6gui6 \
    libqt6widgets6 \
    libqt6network6 \
    qmake6 \
    && rm -rf /var/lib/apt/lists/*

# Install Protobuf
RUN apt-get update && apt-get install -y \
    protobuf-compiler \
    libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

# Install additional compilers and tools
RUN apt-get update && apt-get install -y \
    clang \
    clang-format \
    clang-tidy \
    gdb \
    valgrind \
    ccache \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Copy project files (context is parent directory)
COPY client/ ./client/
COPY third_party/ ./third_party/

# Set working directory to client
WORKDIR /workspace/client

# Set environment variables for Qt
ENV Qt6_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6
ENV QT_QPA_PLATFORM=offscreen

# Default build configuration
ENV BUILD_TYPE=Release
ENV PLATFORM=linux
ENV COMPILER=gcc

RUN rm -rf /workspace/client/build

# Default command - build the project
CMD ["make", "build"]
