FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy protos (relative to build context, which will be the root)
COPY protos/ /app/protos/

# Copy source
COPY compute-cpp/ /app/compute-cpp/

WORKDIR /app/compute-cpp
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

EXPOSE 50051

CMD ["./build/compute_service"]
