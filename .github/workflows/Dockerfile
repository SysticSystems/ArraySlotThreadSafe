FROM alpine:edge

# Install tools - explicitly targeting the versioned package
RUN apk update && apk add --no-cache \
    clang21 \
    clang21-extra-tools \
    compiler-rt \
    cmake \
    ninja \
    py3-pip \
    pipx \
    bash \
    git \
    build-base

# Protocol Handshake: Force system-wide recognition of clang-21
# Alpine often puts these in /usr/bin/ or requires explicit versioned calls
RUN ln -sf /usr/bin/clang-21 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-21 /usr/bin/clang++ && \
    ln -sf /usr/bin/clang-21 /usr/bin/cc && \
    ln -sf /usr/bin/clang++-21 /usr/bin/c++

# Isolated Conan setup
RUN pipx install conan
ENV PATH="/root/.local/bin:${PATH}"

# Export variables for the build environment
ENV CC=clang
ENV CXX=clang++

WORKDIR /workspace

CMD ["/bin/bash", "-c", "./build.sh Testing"]