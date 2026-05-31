# =============================================================================
# Rivet Compiler — Reproducible Build Environment
# =============================================================================
# Build:  docker build -t rivet .
# Run:    docker run -it rivet
# Test:   docker run rivet ./build/rivet tests/validation.rvt --dump-ast
# =============================================================================

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# ---- Core build toolchain + LLVM 18 ----------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        clang-18 \
        llvm-18-dev \
        lld-18 \
        libzstd-dev \
        libffi-dev \
        ca-certificates \
        wget \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ---- ARM bare-metal cross-compilation toolchain -----------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi \
        libnewlib-arm-none-eabi \
        binutils-arm-none-eabi \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ---- Renode (hardware emulator) ---------------------------------------------
ARG RENODE_VERSION=1.16.1
RUN wget -q "https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode_${RENODE_VERSION}_amd64.deb" \
        -O /tmp/renode.deb \
    && apt-get update && apt-get install -y /tmp/renode.deb \
    && rm -f /tmp/renode.deb \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ---- Copy source and build --------------------------------------------------
WORKDIR /rivet
COPY . .

RUN cmake -S . -B build \
      -DCMAKE_C_COMPILER=clang-18 \
      -DCMAKE_CXX_COMPILER=clang++-18 \
      -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# ---- Smoke test — fail the build if the compiler is broken ------------------
RUN ./build/rivet tests/validation.rvt --dump-ast

CMD ["bash"]
