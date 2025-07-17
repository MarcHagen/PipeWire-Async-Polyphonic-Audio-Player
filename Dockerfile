FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ARG DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    pkg-config \
    libpipewire-0.3-dev \
    libspa-0.2-dev \
    pipewire \
    pipewire-audio-client-libraries \
    pipewire-pulse \
    pulseaudio-utils \
    alsa-utils \
    libpipewire-0.3-common \
    libspa-0.2-modules \
    --no-install-recommends && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# For debugging purposes, let's check where the SPA headers are located
RUN find /usr -name "format-utils.h" | grep spa

# Create a working directory
WORKDIR /app

# Copy the source code and build files
COPY multichannel_player.c /app/
COPY Makefile /app/

# Create a modified Makefile that uses pkg-config to find include paths
RUN sed -i 's/CFLAGS = -Wall -Wextra -O2 -g/CFLAGS = -Wall -Wextra -O2 -g $(shell pkg-config --cflags libpipewire-0.3 libspa-0.2)/' Makefile && \
    sed -i 's/LDLIBS = -lpipewire-0.3 -lm/LDLIBS = $(shell pkg-config --libs libpipewire-0.3 libspa-0.2) -lm/' Makefile

# Show the modified Makefile for debugging
RUN cat Makefile

# Build the application
RUN make

# Create a non-root user to run the application
RUN useradd -m appuser
USER appuser

# Set up environment variables for PipeWire
ENV XDG_RUNTIME_DIR=/tmp

USER appuser

# Default command to run the player with test tone
CMD ["/app/multichannel_player", "-t"]
