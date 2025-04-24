#!/bin/bash

BUILD_DIR="build"

show_help() {
    echo "Usage: ./build.sh [OPTION]"
    echo ""
    echo "Options:"
    echo "  --build      Build the project (default if no option is given)"
    echo "  --clean      Remove the build directory"
    echo "  --rebuild    Clean and then build the project"
    echo "  --help       Show this help message"
}

build_project() {
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    cmake ..
    make -j$(nproc)
    cd ..
}

clean_project() {
    echo "Cleaning build directory..."
    rm -rf $BUILD_DIR
}

# Parse argument
case "$1" in
    "" | --build)
        echo "[BUILD] Building the project..."
        build_project
        ;;
    --clean)
        clean_project
        ;;
    --rebuild)
        clean_project
        echo "[REBUILD] Rebuilding the project..."
        build_project
        ;;
    --help)
        show_help
        ;;
    *)
        echo "Unknown option: $1"
        show_help
        ;;
esac
