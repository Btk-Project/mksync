FROM debian:sid

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive
ENV XMAKE_ROOT=y
ENV CC=gcc-16
ENV CXX=g++-16

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        cpp-16 \
        curl \
        file \
        g++-16 \
        gcc-16 \
        git \
        libei-dev \
        libportal-dev \
        libwayland-bin \
        libwayland-dev \
        libxcb1-dev \
        libxcb-keysyms1-dev \
        libxcb-randr0-dev \
        libxcb-xinput-dev \
        libxcb-xtest0-dev \
        libxkbcommon-dev \
        make \
        ninja-build \
        patchelf \
        pkg-config \
        python3 \
        tar \
        unzip \
        wayland-protocols \
        x11proto-dev \
        xmake \
        xz-utils \
        zip \
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
