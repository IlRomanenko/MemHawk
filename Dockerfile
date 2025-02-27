# Use a specific version of Ubuntu
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

# Install GCC 9 and other dependencies
RUN apt-get update -y
RUN apt-get install -y \
    build-essential \
    gcc-9 g++-9 \
    cmake git \
    libtool autoconf unzip wget \
    libboost-container-dev \
    software-properties-common lsb-release

# Install newer version of cmake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt update -y && \
    apt install -y cmake

# Set GCC 9 as default compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 100

# Set the working directory
WORKDIR /workspace

# Copy the source code into container
COPY . .

# Create a build directory and compile project
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/artifacts .. && \
    make -j $(nproc) && \
    make install
