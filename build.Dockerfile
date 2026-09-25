FROM debian:trixie-backports AS base

WORKDIR /rv640

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get --no-install-recommends -y install \
      ca-certificates \
      wget \
      build-essential \
      cmake \
      ninja-build \
      device-tree-compiler \
      file \
      cpio \
      unzip \
      rsync \
      bc \
      binutils \
      diffutils \
      patch \
      gzip \
      bzip2 \
      perl \
      findutils \
      gawk \
      git \
      python3


FROM base AS build-rv640

RUN wget "https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16/+files/binutils-ia16-elf_2.39-20260423.22-ppa260423231~resolute_amd64.deb" && \
    dpkg -i "binutils-ia16-elf_2.39-20260423.22-ppa260423231~resolute_amd64.deb"

RUN wget "https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16/+files/libnewlib-ia16-elf_2.4.0-20260316.15-stage1gcc6.3.0-20260612.00-binutils2.39-20260423.22-ppa260612105~resolute_amd64.deb" && \
    dpkg -i "libnewlib-ia16-elf_2.4.0-20260316.15-stage1gcc6.3.0-20260612.00-binutils2.39-20260423.22-ppa260612105~resolute_amd64.deb"

RUN wget "https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16/+files/libi86-ia16-elf_20260727-stage1gcc6.3.0-20260612.00-binutils2.39-20260423.22-ppa260727151~resolute_amd64.deb" && \
    dpkg -i "libi86-ia16-elf_20260727-stage1gcc6.3.0-20260612.00-binutils2.39-20260423.22-ppa260727151~resolute_amd64.deb"

RUN wget "https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16/+files/gcc-ia16-elf_6.3.0-20260612.00-ppa260612105~resolute_amd64.deb" && \
    dpkg -i "gcc-ia16-elf_6.3.0-20260612.00-ppa260612105~resolute_amd64.deb"

WORKDIR /rv640

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja

RUN cmake --build build --config Release -j


FROM base AS build-linux

WORKDIR /build

RUN wget "https://buildroot.org/downloads/buildroot-2026.05.1.tar.gz" && \
    tar -xzvf buildroot-2026.05.1.tar.gz && \
    mv buildroot-2026.05.1 buildroot

COPY buildroot/ .

RUN cp configs/.config buildroot/

RUN cd buildroot/linux && git apply ../../linux.mk.patch

WORKDIR /build/buildroot

RUN make oldconfig && make


FROM scratch AS dist

COPY --from=build-rv640 /rv640/build/cmake/dos/RV640.exe      /dos/
COPY --from=build-rv640 /rv640/build/cmake/dos/rv640_rv32.dtb /dos/
#COPY --from=build-rv640 /rv640/build/cmake/dos/rv640_rv64.dtb /dos/

COPY --from=build-rv640 /rv640/build/cmake/linux/RV640          /linux/
COPY --from=build-rv640 /rv640/build/cmake/linux/rv640_rv32.dtb /linux/
#COPY --from=build-rv640 /rv640/build/cmake/linux/rv640_rv64.dtb /linux/

COPY --from=build-linux /build/buildroot/output/images/Image  /
