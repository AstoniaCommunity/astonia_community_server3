# Astonia 3 Community Server - Multi-target Dockerfile
#
# Available targets:
#   builder           - Build environment for code customization
#   runtime-base      - Server without zones (for custom zones)
#   runtime-with-zones - Server with default zones (default)
#
# Image tags on DockerHub:
#   astoniacommunity/astonia_server_3:latest  - Ready to play
#   astoniacommunity/astonia_server_3:base    - For custom zones
#   astoniacommunity/astonia_server_3:builder - For code customization
#
# Examples:
#   docker build -t astonia:latest .
#   docker build -t astonia:base --target runtime-base .
#   docker build -t astonia:builder --target builder .
#
# ============================================================================
# Stage 1: Builder - Build environment with all dependencies
# ============================================================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Add i386 architecture and install build dependencies
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y \
        gcc-multilib \
        make \
        lib32z-dev \
        libargon2-dev:i386 \
        libmysqlclient-dev:i386 \
        # Useful tools for development
        git \
        vim \
        less \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source files
COPY *.c *.h *.S Makefile ./
COPY .obj/ .obj/
COPY zones/ zones/

# Create runtime directories that the Makefile expects
RUN mkdir -p runtime/generic runtime/1 runtime/2 runtime/3 runtime/5 \
    runtime/6 runtime/8 runtime/10 runtime/11 runtime/13 runtime/14 \
    runtime/15 runtime/16 runtime/17 runtime/18 runtime/19 runtime/20 \
    runtime/22 runtime/23 runtime/24 runtime/25 runtime/26 runtime/28 \
    runtime/29 runtime/31 runtime/32 runtime/33 runtime/34 runtime/35 \
    runtime/36 runtime/37

# Build the server
RUN make -j$(nproc)

# For the builder image, we want a clean state for rebuilding
# Create a script to help users build
RUN echo '#!/bin/bash\n\
echo "Astonia 3 Build Environment"\n\
echo "============================"\n\
echo ""\n\
echo "To build:"\n\
echo "  make clean && make -j$(nproc)"\n\
echo ""\n\
echo "To copy built files out:"\n\
echo "  # From host: docker cp <container>:/build/server ."\n\
echo ""\n\
echo "Source files are in /build"\n\
exec "$@"' > /entrypoint-builder.sh && chmod +x /entrypoint-builder.sh

ENTRYPOINT ["/entrypoint-builder.sh"]
CMD ["/bin/bash"]

# ============================================================================
# Stage 2: Runtime Base - Server without zones (for custom zone development)
# ============================================================================
FROM ubuntu:22.04 AS runtime-base

ENV DEBIAN_FRONTEND=noninteractive

# Add i386 architecture and install runtime dependencies
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y \
        libc6:i386 \
        libz1:i386 \
        libargon2-1:i386 \
        libmysqlclient21:i386 \
        libstdc++6:i386 \
        libgcc-s1:i386 \
        mysql-client \
        procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /server

# Copy built binaries from builder
COPY --from=builder /build/server .
COPY --from=builder /build/chatserver .
COPY --from=builder /build/create_account .
COPY --from=builder /build/create_character .

# Copy runtime directory (compiled DLLs for each area)
COPY --from=builder /build/runtime/ runtime/

# Create empty zones directory structure (user must mount their own)
RUN mkdir -p zones/generic zones/presets && \
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37; do \
        mkdir -p zones/$i; \
    done

# Copy generated item files (these are needed regardless)
COPY --from=builder /build/zones/generic/weapons.itm zones/generic/
COPY --from=builder /build/zones/generic/armor.itm zones/generic/

# Copy SQL files for database initialization
COPY create_tables.sql merc.sql ./

# Copy entrypoint script
COPY docker-entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Create motd.txt
RUN echo "Welcome to Astonia Community Server" > motd.txt

# Expose ports
EXPOSE 5554
EXPOSE 5556-5590

ENTRYPOINT ["/entrypoint.sh"]
CMD ["start"]

# ============================================================================
# Stage 3: Runtime with Zones - Ready to play (default)
# ============================================================================
FROM runtime-base AS runtime-with-zones

# Copy all zone data from builder (overwrites empty structure)
COPY --from=builder /build/zones/ zones/
