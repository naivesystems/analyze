FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
autoconf \
automake \
bear \
build-essential \
cgroup-tools \
clang \
clang-tidy \
clang-tools \
cmake \
diffutils \
dnsutils \
file \
findutils \
flex \
g++ \
gcc \
gcc-arm-none-eabi \
gcc-multilib \
gettext \
git \
gnulib \
libcgroup-dev \
libssl-dev \
libtool \
libzstd-dev \
lld \
llvm \
llvm-dev \
lzip \
make \
pip \
pkg-config \
procps \
python3-dev \
sudo \
texinfo \
tzdata \
unzip \
vim \
vim-common \
wget \
xz-utils \
zlib1g.dev \
&& apt-get clean && rm -rf /var/lib/apt/lists/*

RUN cd /tmp \
&& wget https://go.dev/dl/go1.21.4.linux-amd64.tar.gz \
&& rm -rf /usr/local/go \
&& tar -C /usr/local -xzf go1.21.4.linux-amd64.tar.gz

RUN cd /tmp \
&& wget https://github.com/bazelbuild/bazel/releases/download/6.4.0/bazel-6.4.0-linux-x86_64 \
&& mv bazel-6.4.0-linux-x86_64 /usr/local/bin/bazel \
&& chmod +x /usr/local/bin/bazel

ARG USERNAME=build
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
&& useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
&& echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
&& chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME
WORKDIR /home/$USERNAME

RUN git clone https://github.com/naivesystems/analyze.git
WORKDIR /home/$USERNAME/analyze

ENV PATH=/usr/local/go/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN make
RUN sed -i 's/podman build/@echo podman build/g' podman_image/Makefile
RUN sed -i 's/rm -rf tmpWorkSpace/@echo/g' podman_image/Makefile
RUN make -C podman_image build-en
