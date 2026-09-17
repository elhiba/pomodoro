# A complete build and test environment for the app, so a contributor can compile it and
# run the tests against their own checkout without installing Qt on their machine.
#
# Debian trixie rather than an Ubuntu LTS on purpose: CMakeLists.txt asks for Qt 6.5 or
# newer, and Ubuntu 22.04 ships 6.2 while 24.04 ships 6.4 -- both too old. Trixie carries
# Qt 6.8 in its own archive, so every dependency comes from apt at a pinned distribution
# version instead of being downloaded from elsewhere at build time.
#
# The image holds the toolchain only. The source is bind-mounted at run time (see
# docker-compose.yml), so editing a file on the host and re-running the tests does not
# rebuild the image.

FROM debian:trixie-slim

# Never let apt stop to ask a question during an image build.
ENV DEBIAN_FRONTEND=noninteractive

# One layer, and the package lists are dropped in the same step so they are not left
# behind in the image.
#
# Split into three groups on purpose:
#   * the toolchain CMakeLists.txt needs,
#   * the Qt development packages, one per component in its find_package call,
#   * the QML modules and X libraries, which are only needed to actually open the window
#     -- the tests are headless and would not miss them, but leaving them out would make
#     `make run` fail with an import error that is tedious to diagnose.
RUN apt-get update && apt-get install --no-install-recommends --yes \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        ca-certificates \
        \
        qt6-base-dev \
        qt6-base-dev-tools \
        qt6-declarative-dev \
        qt6-declarative-dev-tools \
        qt6-multimedia-dev \
        qt6-svg-dev \
        libgl1-mesa-dev \
        libxkbcommon-dev \
        libdbus-1-dev \
        \
        qml6-module-qtquick \
        qml6-module-qtquick-controls \
        qml6-module-qtquick-templates \
        qml6-module-qtquick-window \
        qml6-module-qtqml-workerscript \
        libxcb-cursor0 \
        libxkbcommon-x11-0 \
        libgl1 \
        dbus-x11 \
        fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*

# Qt Quick is told to draw on the CPU here for the same reason main.cpp defaults to it:
# a container has no GPU worth using, and without this the app falls back through a
# missing OpenGL context and dies with an error that has nothing to do with the code.
ENV QT_QUICK_BACKEND=software

# What the tests run under. Setting it in the image means a contributor who runs ctest by
# hand inside the container gets the same headless behaviour CI does.
ENV QT_QPA_PLATFORM=offscreen

# Deliberately NOT "build": a checkout made on Windows or macOS already has a build/
# directory full of that platform's object files, and pointing the container at the same
# path would have the two toolchains overwrite each other's CMake cache.
ENV BUILD_DIR=/work/build-docker

WORKDIR /work

COPY docker/entrypoint.sh /usr/local/bin/entrypoint
RUN chmod +x /usr/local/bin/entrypoint

ENTRYPOINT ["/usr/local/bin/entrypoint"]
CMD ["test"]
