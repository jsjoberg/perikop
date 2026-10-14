# Upstream access is confined to the manual dependency preparation workflow.
FROM rockylinux:9
# Keep documentation: some libraries the AppImage bundles carry their licence text only as docs.
RUN sed -i '/^tsflags=nodocs/d' /etc/dnf/dnf.conf && \
    dnf reinstall -y $(rpm -qa --qf '%{NAME}\n' | grep -v '^gpg-pubkey$') && \
    dnf install -y epel-release dnf-plugins-core && \
    dnf config-manager --set-enabled crb && \
    dnf install -y gcc-toolset-15-gcc gcc-toolset-15-gcc-c++ make cmake ninja-build ccache \
      git curl-minimal tar xz zstd gtk3-devel fontconfig-devel xorg-x11-server-Xvfb \
      xorg-x11-xauth python3.12 binutils file findutils which gdb && \
    dnf clean all && \
    ln -s /usr/bin/python3.12 /usr/local/bin/python3
ENV PATH="/opt/rh/gcc-toolset-15/root/usr/bin:${PATH}"
WORKDIR /work
