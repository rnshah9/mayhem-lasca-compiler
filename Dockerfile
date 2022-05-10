# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install  -y vim less man wget tar git gzip unzip make cmake software-properties-common curl 

RUN curl -sSL https://get.haskellstack.org/ | sh

RUN apt install -y llvm-6.0-dev libgc-dev zlib1g-dev cmake
RUN add-apt-repository universe
RUN apt install -y libpcre2-dev

ADD . /lasca-compiler
ENV LASCAPATH="/lasca-compiler/libs/base"
ENV PATH=$PATH:~/.local/bin

WORKDIR /lasca-compiler
RUN stack setup
RUN make build -j8
