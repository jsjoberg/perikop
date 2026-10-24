# Upstream access is confined to the manual dependency preparation workflow.
FROM debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl g++ make cmake ninja-build ccache pkg-config git tar xz-utils zstd \
      libgtk-3-dev libfontconfig-dev librsvg2-common xvfb xauth python3 binutils file findutils gdb \
      libicu76 squashfs-tools strace dpkg-dev clang-tidy-22 && \
    rm -rf /var/lib/apt/lists/* && \
    ln -s /usr/bin/clang-tidy-22 /usr/local/bin/clang-tidy && \
    ln -s /usr/bin/run-clang-tidy-22 /usr/local/bin/run-clang-tidy
# Velopack's vpk needs the .NET 8 runtime, which Debian does not package.
RUN case $(dpkg --print-architecture) in \
      amd64) rid=x64 sha512=639a17d84e4d52fecd62c7d4e528e7bc0ae3c9860de2530cb41a2ee7196b69ea18271d8795849180742770e25b4f913df292a5a236fa197cf5d3cd0ec5ba665f ;; \
      arm64) rid=arm64 sha512=234372d5af973e23958b209eeccbee35f4209af98b7ecc93f16488869eef0729aa383cee5c6cc23dd836cbde88090ec5df0c317d39d3e17e09051403392ea9cd ;; \
    esac && \
    curl -fsSL -o /tmp/dotnet.tar.gz \
      "https://builds.dotnet.microsoft.com/dotnet/Runtime/8.0.32/dotnet-runtime-8.0.32-linux-$rid.tar.gz" && \
    echo "$sha512  /tmp/dotnet.tar.gz" | sha512sum -c - && \
    mkdir -p /opt/dotnet && tar -xzf /tmp/dotnet.tar.gz -C /opt/dotnet && rm /tmp/dotnet.tar.gz && \
    ln -s /opt/dotnet/dotnet /usr/local/bin/dotnet
ENV DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
WORKDIR /work
