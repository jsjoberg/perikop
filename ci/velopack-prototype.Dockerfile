# Manual compatibility research only; production builds keep using pinned snapshots.
# EL9's glibc 2.34 is Perikop's Linux baseline on both supported architectures.
FROM rockylinux:9
RUN dnf install -y gcc-c++ binutils file python3.12 dotnet-sdk-8.0 squashfs-tools strace findutils which && \
    dnf clean all
ENV DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
WORKDIR /work
