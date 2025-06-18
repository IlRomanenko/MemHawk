# Use a specific version of Ubuntu
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y
RUN apt-get install -y \
    git ccache \
    libtool autoconf unzip wget \
    libboost-container-dev \
    software-properties-common lsb-release

RUN wget -qO- https://apt.llvm.org/llvm.sh | bash -s 18
RUN apt-get install -y clang-18 lldb-18 libc++-18-dev libc++abi-18-dev clang-tools-18

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100

# Install newer version of cmake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt update -y && \
    apt install -y cmake

# Set the working directory
WORKDIR /workspace

# Copy the source code into container
COPY . .

# Create a build directory and compile project
RUN \
    cmake -B build --preset ReleaseClang -DCMAKE_INSTALL_PREFIX=/artifacts && \
    cmake --build build --parallel $(nproc) && \
    cd build && make install
