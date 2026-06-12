# Use a specific version of Ubuntu
FROM ubuntu:18.04
ARG DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

RUN apt-get update -y
RUN apt-get install -y \
    git ccache \
    libtool autoconf unzip wget \
    software-properties-common lsb-release \ 
    curl ninja-build

# Install gcc-11
RUN add-apt-repository ppa:ubuntu-toolchain-r/test && apt update
RUN apt install -y gcc-11 g++-11

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

# Install clang-18
RUN wget -qO- https://apt.llvm.org/llvm.sh | bash -s 18
RUN apt-get install -y clang-18 lldb-18 libc++-18-dev libc++abi-18-dev clang-tools-18

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100

# Install newer version of cmake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt update -y && \
    apt install -y cmake

# Install newer version of rust
RUN curl https://sh.rustup.rs -sSf | sh -s -- --default-toolchain stable -y
ENV PATH=/root/.cargo/bin:$PATH

# Set the working directory
WORKDIR /workspace

# Copy the source code into container
COPY . .

# Create a build directory and compile project
RUN cmake -B build --preset Release -DCMAKE_INSTALL_PREFIX=/artifacts
# Build libunwind and xxhash manually, otherwise memhawk will fail to build on old ubuntu version
RUN cmake --build build --parallel $(nproc) -t libunwind_src -t xxhash_src
RUN cmake --build build --parallel $(nproc) -t memhawk_all && \
    cmake --build build -t install
