FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates pkg-config \
    qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libqt6charts6-dev libqt6core5compat6-dev qt6-declarative-dev qt6-svg-dev \
    libqt6sql6-sqlite libqt6svg6 libquazip1-qt6-dev libzint-dev zlib1g-dev \
    libgl1-mesa-dev libxkbcommon-x11-0 xvfb fonts-dejavu-core \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
