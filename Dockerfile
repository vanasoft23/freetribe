FROM debian:bookworm

RUN apt-get update && \
    apt-get clean && \ 
    apt-get install -y --no-install-recommends \
    build-essential \
    gcc-arm-none-eabi \
    gdb-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    cmake \
    ninja-build \
    openocd \
#    dfu-util \ # <-- do we include this?
    python3 \
    python3-pip \
    git \
    wget \
    ftp \
    xxd \
    neovim \
    sudo

# # Mac OS fix
# RUN dpkg --add-architecture amd64 && \
#     apt-get update && \
#     apt-get install -y libc6:amd64

RUN wget --progress=dot:giga --no-check-certificate -O /tmp/blackfin-toolchain.tar.bz2 \
    https://sourceforge.net/projects/adi-toolchain/files/2014R1/2014R1-RC2/x86_64/blackfin-toolchain-elf-gcc-4.3-2014R1-RC2.x86_64.tar.bz2/download && \
    tar -xjvf /tmp/blackfin-toolchain.tar.bz2 -C / && \
    rm /tmp/blackfin-toolchain.tar.bz2

# Add openocd-bfin

ENV PATH "/opt/uClinux/bfin-elf/bin/:$PATH"
ENV LIB_GCC="/usr/lib/gcc/arm-none-eabi/12.2.1/"
ENV LIB_C="/usr/lib/arm-none-eabi/newlib/"
ENV BFIN_TOOLCHAIN="/opt/uClinux/bfin-elf/bin/"

SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN useradd -G sudo -ms /bin/bash user && \
    echo "user:password" | chpasswd && \
    echo "ALL ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers && \
    echo "alias python=python3" >> /home/user/.bashrc && \
    echo "alias ll='ls -lah'" >> /home/user/.bashrc && \
    echo "alias vim=nvim" >> /home/user/.bashrc

USER user
WORKDIR /freetribe

ENTRYPOINT ["/bin/bash"]
