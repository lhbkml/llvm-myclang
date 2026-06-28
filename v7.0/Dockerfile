# myclang-c++ Docker 镜像
#
# 构建:  docker build -t myclang-cc .
# 运行:
#   docker run --rm myclang-cc /app/output/test.c
#   docker run --rm -v $(pwd):/data myclang-cc /data/test.c --json
#   cat test.c | docker run --rm -i myclang-cc --stdin --json

FROM ubuntu:noble AS builder

# Ubuntu 24.04 自带 LLVM 18
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make llvm-18-dev libclang-18-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN make clean && make frontendAction

# ============================================================
FROM ubuntu:noble

RUN apt-get update && apt-get install -y --no-install-recommends \
    libclang-cpp18 libllvm18 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/frontendAction /src/output ./output/
COPY --from=builder /src/frontendAction .
RUN chmod +x frontendAction

ENTRYPOINT ["./frontendAction"]
