FROM debian:sid

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive
ENV XMAKE_ROOT=y
ENV CC=gcc
ENV CXX=g++

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        curl \
        file \
        g++-16 \
        gcc-16 \
        git \
        libportal-dev \
        libx11-dev \
        libxcb-keysyms1-dev \
        libxcb-util0-dev \
        libxcb-xtest0-dev \
        libxi-dev \
        make \
        ninja-build \
        patchelf \
        pkg-config \
        python3 \
        tar \
        unzip \
        xmake \
        xz-utils \
        zip \
    && apt-get purge -y --auto-remove \
        cpp \
        cpp-15 \
        g++ \
        g++-15 \
        gcc \
        gcc-15 \
        libgcc-15-dev \
        libstdc++-15-dev \
    && ln -sf /usr/bin/gcc-16 /usr/local/bin/gcc \
    && ln -sf /usr/bin/g++-16 /usr/local/bin/g++ \
    && ln -sf /usr/bin/gcc-16 /usr/local/bin/cc \
    && ln -sf /usr/bin/g++-16 /usr/local/bin/c++ \
    && ln -sf /usr/bin/cpp-16 /usr/local/bin/cpp \
    && ln -sf /usr/bin/gcov-16 /usr/local/bin/gcov \
    && ln -sf /usr/bin/gcc-ar-16 /usr/local/bin/gcc-ar \
    && ln -sf /usr/bin/gcc-nm-16 /usr/local/bin/gcc-nm \
    && ln -sf /usr/bin/gcc-ranlib-16 /usr/local/bin/gcc-ranlib \
    && gcc --version \
    && g++ --version \
    && xmake --version \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd --gid "${USER_GID}" "${USERNAME}" \
    && useradd --uid "${USER_UID}" --gid "${USER_GID}" --create-home --shell /bin/bash "${USERNAME}"

WORKDIR /workspace/mksync

CMD ["bash"]
