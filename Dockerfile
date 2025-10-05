# Base image for Adaptive AUTOSAR development environment
FROM ubuntu:22.04

# Install essential packages for Adaptive AUTOSAR development
# Including build tools, compilers, and AUTOSAR-specific dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        # Build essentials
        build-essential \
        gcc-11 \
        g++-11 \
        gdb \
        make \
        cmake \
        ninja-build \
        # Version control
        git \
        git-lfs \
        # Python and tools
        python3 \
        python3-pip \
        python3-dev \
        python3-setuptools \
        python3-wheel \
        python3-yaml \
        python3-jinja2 \
        # Libraries for AUTOSAR
        libboost-all-dev \
        libssl-dev \
        libcurl4-openssl-dev \
        libxml2-dev \
        libxslt1-dev \
        libjsoncpp-dev \
        libprotobuf-dev \
        protobuf-compiler \
        libgrpc++-dev \
        protobuf-compiler-grpc \
        # DDS middleware (for SOME/IP alternative)
        libdds-dev \
        # Networking tools
        iproute2 \
        net-tools \
        iputils-ping \
        tcpdump \
        wireshark-common \
        # Documentation tools
        doxygen \
        graphviz \
        plantuml \
        # Utilities
        wget \
        curl \
        unzip \
        xz-utils \
        file \
        locales \
        sudo \
        vim \
        nano \
        libreadline-dev \
        # Code quality tools
        clang-format \
        clang-tidy \
        cppcheck \
        valgrind \
        lcov && \
    rm -rf /var/lib/apt/lists/*

# Configure locale for proper character encoding
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# Install Python packages for AUTOSAR tools
RUN pip3 install --no-cache-dir \
    pyyaml \
    jinja2 \
    lxml \
    numpy \
    matplotlib \
    pandas \
    pytest \
    pylint \
    black \
    autopep8 \
    cantools \
    python-can \
    scapy \
    pyserial \
    autosar \
    pydantic

# Set GCC 11 as default compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

# Create directories for IPC sockets with proper permissions
# This enables Unix domain socket tests to work properly
RUN mkdir -p /var/run/cdmf /tmp/cdmf && \
    chmod 777 /var/run/cdmf /tmp/cdmf && \
    mkdir -p /workspace/build

# Create working directory
WORKDIR /workspace

# Volume mount points
VOLUME ["/workspace"]
VOLUME ["/var/run/cdmf"]

# Usage instructions:
# Build: docker build -t adaptive-autosar-dev .
#
# Standard Run (without socket tests):
# Run:   docker run -it \
#          -v $(pwd):/workspace \
#          adaptive-autosar-dev
#
# Run with Unix Socket Support (for IPC tests):
# Run:   docker run -it \
#          -v $(pwd):/workspace \
#          --ipc=host \
#          adaptive-autosar-dev
#
# For Windows PowerShell (with socket support):
# Run:   docker run -it `
#          -v ${PWD}:/workspace `
#          --ipc=host `
#          adaptive-autosar-dev
#
# To run socket tests inside container:
#   cd /workspace/workspace
#   ./build.sh --enable-socket-tests
#
# Or run specific socket test:
#   cd /workspace/workspace/build
#   ENABLE_SOCKET_TESTS=1 ./bin/test_proxy_stub

# Set entrypoint to initialize directories and run command
CMD ["/bin/bash"]