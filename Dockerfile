FROM ubuntu:24.04

LABEL maintainer="mmgul562"

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    cmake \
    libpq-dev \
    libargon2-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY src ./src

RUN mkdir build