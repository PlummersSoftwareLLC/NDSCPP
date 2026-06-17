#!/usr/bin/env bash
# Checks that all build and test dependencies are present.
# Exits 0 if everything is in place; exits 1 and prints a diagnosis otherwise.

set -euo pipefail

PLATFORM=$(uname -s)
MISSING_BUILD=()
MISSING_TEST=()
MISSING_CPR_SOURCE=false

check_brew() {
    brew list "$1" &>/dev/null || MISSING_BUILD+=("$1")
}

check_brew_test() {
    brew list "$1" &>/dev/null || MISSING_TEST+=("$1")
}

check_apt() {
    dpkg -s "$1" &>/dev/null || MISSING_BUILD+=("$1")
}

check_apt_test() {
    dpkg -s "$1" &>/dev/null || MISSING_TEST+=("$1")
}

if [[ "$PLATFORM" == "Darwin" ]]; then
    echo "Platform: macOS"
    for pkg in asio ffmpeg ncurses spdlog; do
        check_brew "$pkg"
    done
    check_brew_test googletest
    check_brew_test cpr
else
    echo "Platform: Linux"
    for pkg in libasio-dev zlib1g-dev libavformat-dev libavcodec-dev \
                libavutil-dev libswscale-dev libswresample-dev \
                libcurl4-gnutls-dev libspdlog-dev; do
        check_apt "$pkg"
    done
    check_apt_test libgtest-dev
    check_apt_test cmake

    # cpr has no apt package on Ubuntu; cmake installs to /usr/local/lib which
    # may not be registered in the ldconfig cache, so check both.
    if ! ldconfig -p 2>/dev/null | grep -qE '^\s+libcpr\.so' && \
       ! ls /usr/local/lib/libcpr.so* &>/dev/null; then
        MISSING_CPR_SOURCE=true
    fi
fi

RC=0

if [[ ${#MISSING_BUILD[@]} -gt 0 ]]; then
    echo ""
    echo "MISSING build dependencies: ${MISSING_BUILD[*]}"
    if [[ "$PLATFORM" == "Darwin" ]]; then
        echo "  Install with: brew install ${MISSING_BUILD[*]}"
    else
        echo "  Install with: sudo apt install ${MISSING_BUILD[*]}"
    fi
    RC=1
fi

if [[ ${#MISSING_TEST[@]} -gt 0 ]]; then
    echo ""
    echo "MISSING test dependencies: ${MISSING_TEST[*]}"
    if [[ "$PLATFORM" == "Darwin" ]]; then
        echo "  Install with: brew install ${MISSING_TEST[*]}"
    else
        echo "  Install with: sudo apt install ${MISSING_TEST[*]}"
    fi
    RC=1
fi

if [[ "$MISSING_CPR_SOURCE" == "true" ]]; then
    echo ""
    echo "MISSING: cpr (C++ Requests library) — no apt package available on Ubuntu."
    echo "  Build and install it manually:"
    echo "    git clone https://github.com/libcpr/cpr.git"
    echo "    cd cpr && mkdir build && cd build"
    echo "    cmake .. -DCPR_USE_SYSTEM_CURL=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=20"
    echo "    cmake --build . --parallel"
    echo "    sudo cmake --install ."
    RC=1
fi

if [[ $RC -eq 0 ]]; then
    echo ""
    echo "All dependencies are present."
fi

exit $RC
