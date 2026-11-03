# GBSC PRO development image: every toolchain the builds and tests need, pinned, for linux/amd64 and linux/arm64.
# Published by .github/workflows/dev-image.yml as ghcr.io/brisma/gbsc-pro-dev:<the tag in compose.yaml>; compose.yaml
# rebuilds it from here (build context: the repository root, see .dockerignore). Layers: what changes least first.
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
ARG TARGETARCH
ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential pkg-config python3 python3-venv git ca-certificates curl xz-utils util-linux \
      libwebkit2gtk-4.1-dev libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev libudev-dev \
    && rm -rf /var/lib/apt/lists/*

# Node.js from nodejs.org, checksum pinned
ARG NODE_VERSION=26.13.0
RUN case "$TARGETARCH" in \
      amd64) a=x64;   sum=d28a96af6c38596f6d9d97c0ee531c13d238d757742387d1928de822d959e997 ;; \
      arm64) a=arm64; sum=406fe3fbde1c054adff8b297c16fdb5af6659db327c1ac8acd46b56ef1af9a92 ;; \
      *) echo "unsupported architecture $TARGETARCH"; exit 1 ;; esac \
    && curl -fsSLo /tmp/node.tar.xz "https://nodejs.org/dist/v$NODE_VERSION/node-v$NODE_VERSION-linux-$a.tar.xz" \
    && echo "$sum  /tmp/node.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/node.tar.xz -C /usr/local --strip-components=1 --exclude=CHANGELOG.md --exclude=LICENSE --exclude=README.md \
    && rm /tmp/node.tar.xz

# Playwright's browsers and their system packages; the version must equal @playwright/test in
# gbs-control/public/package-lock.json (tools/dev.py doctor checks it)
ARG PLAYWRIGHT_VERSION=1.64.1
ENV PLAYWRIGHT_BROWSERS_PATH=/ms-playwright PLAYWRIGHT_VERSION=$PLAYWRIGHT_VERSION
RUN npx -y playwright@$PLAYWRIGHT_VERSION install --with-deps chromium firefox webkit \
    && rm -rf /var/lib/apt/lists/* /root/.npm

# Arm GNU Toolchain (the ADV Controller), checksum pinned
ARG ARM_GNU=15.3.rel2
RUN case "$TARGETARCH" in \
      amd64) a=x86_64;  sum=463df9665c246fa13bab2102115bbf83440661b6eb70b44f3dfd4712ca30037a ;; \
      arm64) a=aarch64; sum=a0dc18893171d39dec82fd8d57eeab3da836fd3df4b61978db3eccbf4a6bb948 ;; \
      *) echo "unsupported architecture $TARGETARCH"; exit 1 ;; esac \
    && curl -fsSLo /tmp/arm.tar.xz "https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/$ARM_GNU/arm-gnu-toolchain-$ARM_GNU-$a-arm-none-eabi.tar.xz" \
    && echo "$sum  /tmp/arm.tar.xz" | sha256sum -c - \
    && mkdir -p /opt/arm && tar -xJf /tmp/arm.tar.xz -C /opt/arm --strip-components=1 && rm /tmp/arm.tar.xz
ENV PATH=/opt/arm/bin:$PATH

# Python tools in one venv first on PATH: PlatformIO and the packages of the build scripts. The platforms' toolchains
# are installed here from platformio.ini (a new platform version there: bump the image tag in compose.yaml); the
# lib_deps download on the first build into gbs-control/.pio/libdeps of the mounted repository.
ENV VIRTUAL_ENV=/opt/venv PATH=/opt/venv/bin:$PATH PLATFORMIO_CORE_DIR=/opt/pio
COPY gbs-control/platformio.ini /tmp/pio/platformio.ini
RUN python3 -m venv /opt/venv \
    && pip install --no-cache-dir platformio==6.2.0 pyyaml==6.0.3 fonttools==4.62.1 brotli==1.2.1 \
    && pio pkg install -d /tmp/pio -e gbsc-pro -e native \
    && rm -rf /tmp/pio && chmod -R a+rwX /opt/pio

# Rust (minimal profile, pinned; rustup-init checksum pinned); the cargo registry and git caches are volumes
# (compose.yaml)
ENV RUSTUP_HOME=/opt/rustup CARGO_HOME=/opt/cargo PATH=/opt/cargo/bin:$PATH
ARG RUST_VERSION=1.99.0
ARG RUSTUP_VERSION=1.29.2
RUN case "$TARGETARCH" in \
      amd64) t=x86_64-unknown-linux-gnu;  sum=5be191e8c70a5007074254cb74d78dbfdd25cd5a61c27de7b8e5410122ec24f4 ;; \
      arm64) t=aarch64-unknown-linux-gnu; sum=a9c168dbea81a84671c6b7a5c1795cb0eb699020110d107ddfca427bd797e697 ;; \
      *) echo "unsupported architecture $TARGETARCH"; exit 1 ;; esac \
    && curl -fsSLo /tmp/rustup-init "https://static.rust-lang.org/rustup/archive/$RUSTUP_VERSION/$t/rustup-init" \
    && echo "$sum  /tmp/rustup-init" | sha256sum -c - \
    && chmod +x /tmp/rustup-init && /tmp/rustup-init -y --no-modify-path --profile minimal --default-toolchain $RUST_VERSION \
    && rm /tmp/rustup-init && chmod -R a+rwX /opt/rustup /opt/cargo

# The devcontainer user (VS Code matches its uid to the host's); compose run uses the entrypoint's uid instead. The
# volume mount points belong to it: a new named volume takes the ownership of the image's directory, and the
# devcontainer (overrideCommand) never runs the entrypoint
RUN usermod -l dev -d /home/dev -m ubuntu && groupmod -n dev ubuntu \
    && mkdir -p /cache/pio-build /cache/target /opt/cargo/registry /opt/cargo/git \
                /work/gbs-control/public/node_modules /work/gbsc-pro-flasher/ui/node_modules \
    && chown dev:dev /cache/pio-build /cache/target /opt/cargo/registry /opt/cargo/git \
                     /work/gbs-control/public/node_modules /work/gbsc-pro-flasher/ui/node_modules
COPY docker/entrypoint.sh /usr/local/bin/gbsc-entrypoint
ENV GBSC_DEV_CONTAINER=1
# last: the hash of the image's inputs (dev-image.yml) changes with every input, it must not void the cached layers
ARG GBSC_DEV_INPUTS=local
LABEL gbsc.dev.inputs=$GBSC_DEV_INPUTS org.opencontainers.image.source=https://github.com/brisma/gbsc-pro
WORKDIR /work
ENTRYPOINT ["/usr/local/bin/gbsc-entrypoint"]
CMD ["bash"]
