# Builder stage
FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    pkg-config \
    libpipewire-0.3-dev \
    libspa-0.2-dev \
    libyaml-dev \
    libsndfile1-dev \
    --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy build files and sources
COPY Makefile /src/Makefile
COPY service/ /src/service/
COPY client/ /src/client/

# Build optimized binaries for Service (papad) and Client (papa)
RUN make release

# Runtime stage
FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    pipewire \
    libpipewire-0.3-0 \
    libpipewire-0.3-common \
    libspa-0.2-modules \
    libyaml-0-2 \
    libsndfile1 \
    --no-install-recommends && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create non-root papa and runtime directory
RUN useradd --create-home --home-dir /home/papa --uid 1001 -s /bin/sh papa \
  && echo "XDG_RUNTIME_DIR=/run/user/1001; export XDG_RUNTIME_DIR" >> /home/papa/.profile \
  && mkdir -m 0700 -p /run/user/1001/papa \
  && chown -R papa:papa /run/user/1001 /home/papa

USER papa
ENV XDG_RUNTIME_DIR=/run/user/1001

# Copy built binaries
COPY --from=builder /src/bin/papad /usr/local/bin/papad
COPY --from=builder /src/bin/papa /usr/local/bin/papa

WORKDIR /app

# Default: run the service
CMD ["papad"]
