#!/bin/bash

REPO_URL="https://github.com/mohammadamin382/BoxcLang.git"
REPO_BRANCH="master"

set -e

echo "╔══════════════════════════════════════════════════════════╗"
echo "║          Box Compiler Setup Script v0.1.0                ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/os-release ]; then
            . /etc/os-release
            echo "$ID"
        elif [ -f /etc/debian_version ]; then
            echo "debian"
        elif [ -f /etc/redhat-release ]; then
            echo "rhel"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

install_dependencies() {
    local os=$1
    echo "[2/8] Installing dependencies for $os..."
    
    case $os in
        ubuntu|debian|pop)
            sudo apt-get update
            sudo apt-get install -y cmake g++ llvm-17 llvm-17-dev make libncurses-dev zlib1g-dev libzstd-dev || \
            sudo apt-get install -y cmake g++ llvm-16 llvm-16-dev make libncurses-dev zlib1g-dev libzstd-dev || \
            sudo apt-get install -y cmake g++ llvm-15 llvm-15-dev make libncurses-dev zlib1g-dev libzstd-dev || \
            sudo apt-get install -y cmake g++ llvm-14 llvm-14-dev make libncurses-dev zlib1g-dev libzstd-dev
            ;;
        fedora|rhel|centos)
            sudo dnf install -y cmake gcc-c++ llvm-devel make ncurses-devel zlib-devel libzstd-devel || \
            sudo yum install -y cmake gcc-c++ llvm-devel make ncurses-devel zlib-devel libzstd-devel
            ;;
        arch|manjaro)
            sudo pacman -Sy --noconfirm cmake gcc llvm make ncurses zlib zstd
            ;;
        opensuse*)
            sudo zypper install -y cmake gcc-c++ llvm-devel make ncurses-devel zlib-devel libzstd-devel
            ;;
        macos)
            if ! command -v brew &> /dev/null; then
                echo "Homebrew not found. Installing Homebrew..."
                /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
            fi
            brew install cmake llvm@17 make ncurses zlib zstd || \
            brew install cmake llvm@16 make ncurses zlib zstd || \
            brew install cmake llvm make ncurses zlib zstd
            ;;
        *)
            echo "Unknown OS. Please install manually: cmake, g++/clang++, llvm (17+), make, ncurses, zlib, zstd"
            exit 1
            ;;
    esac
}

OS=$(detect_os)
echo "Detected OS: $OS"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$(basename "$SCRIPT_DIR")" = "BoxcLang" ]; then
    echo "[1/8] Running from BoxcLang directory, continuing..."
    cd "$SCRIPT_DIR"
else
    if [ -d "BoxcLang" ]; then
        echo "[1/8] Repository exists, skipping clone..."
        cd BoxcLang
    else
        echo "[1/8] Cloning repository..."
        git clone -b "$REPO_BRANCH" "$REPO_URL" BoxcLang
        cd BoxcLang
    fi
fi

install_dependencies "$OS"

echo "[3/8] Configuring CMake..."
cmake -S . -B build

echo "[4/8] Building project (initial build may fail, expected)..."
cd build
make || true

echo "[5/8] Building boxc compiler..."
make boxc

echo "[6/8] Setting up permanent PATH..."
BOXC_PATH="$(pwd)/boxc"
SHELL_RC=""

if [ -n "$BASH_VERSION" ]; then
    SHELL_RC="$HOME/.bashrc"
elif [ -n "$ZSH_VERSION" ]; then
    SHELL_RC="$HOME/.zshrc"
else
    SHELL_RC="$HOME/.profile"
fi

if ! grep -q "export PATH=.*$(dirname "$BOXC_PATH")" "$SHELL_RC" 2>/dev/null; then
    echo "export PATH=\"$(dirname "$BOXC_PATH"):\$PATH\"" >> "$SHELL_RC"
    echo "Added to $SHELL_RC"
fi

export PATH="$(dirname "$BOXC_PATH"):$PATH"

echo "[7/8] Running tests..."
cd ..

ctest --test-dir build --output-on-failure || echo "Some tests failed (check logs above)"

echo "[8/8] Cleaning up temporary files..."
find . -name "*.o" -delete
find . -name "*.ll" -delete
find . -name "*.s" -delete
find ./tests -type f -executable -delete 2>/dev/null || true

echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║              Setup Complete - Good Luck!                 ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
echo "Compiler location: $BOXC_PATH"
echo "Run 'source $SHELL_RC' or restart shell to use 'boxc' command"
echo ""
