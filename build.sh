#!/bin/bash

set -e  # Exit on error

BUILD_DIR="build"

function build() {
    echo "[INFO] Starting build..."
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir "$BUILD_DIR"
        echo "[INFO] Created build directory."
    fi
    cd "$BUILD_DIR"
    echo "[INFO] Running CMake..."
    cmake ..
    echo "[INFO] Building project..."
    make -j$(nproc)
    echo "[SUCCESS] Build complete. Executables are in ./build/"
}

function docker_build() {
    echo "[INFO] Building Docker image 'fuzz_base'..."
    sudo docker build -t fuzz_base .
    echo "[SUCCESS] Docker image 'fuzz_base' built."
}

function clean() {
    echo "[INFO] Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "[SUCCESS] Cleaned."
}

function status() {
    echo "[INFO] Showing Docker container status..."
    docker ps -a --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
}

# Main
case "$1" in
    build)
        build
        ;;
    clean)
        clean
        ;;
    docker_build)
        docker_build
        ;;
    status)
        status
        ;;
    build_all)
        docker_build
        build
        ;;
    *)
        echo "Usage: $0 {build|docker_build|build_all|clean|status}"
        exit 1
        ;;
esac
