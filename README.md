# memhawk

todo:

### Compiling
    cd memhawk # i.e. the source folder
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release .. # look for messages about missing dependencies!
    make -j$(nproc)
