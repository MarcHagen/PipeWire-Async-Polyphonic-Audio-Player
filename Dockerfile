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
    libpipewire-0.3-common \
    libspa-0.2-modules \
    --no-install-recommends && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create a working directory
WORKDIR /app

# Copy the source code and build files
COPY multichannel_player.c /app/
COPY Makefile /app/

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
