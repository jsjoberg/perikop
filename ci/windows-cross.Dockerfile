# This image is prepared manually, then saved and pinned in the dependency snapshot.
FROM debian:13-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl xz-utils cmake ninja-build ccache git libtinfo6 && \
    rm -rf /var/lib/apt/lists/* && \
    git config --global --add safe.directory /work
# Same llvm-mingw release as the cross-build spike. The archive runs on x86_64 Linux.
RUN curl --fail --location --retry 4 --retry-all-errors \
      --output /opt/llvm-mingw.tar.xz \
      https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64.tar.xz && \
    echo 'bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21  /opt/llvm-mingw.tar.xz' | sha256sum --check && \
    mkdir /opt/llvm-mingw && \
    tar -xJf /opt/llvm-mingw.tar.xz --strip-components=1 -C /opt/llvm-mingw && \
    rm /opt/llvm-mingw.tar.xz
ENV PATH="/opt/llvm-mingw/bin:${PATH}"
WORKDIR /work
