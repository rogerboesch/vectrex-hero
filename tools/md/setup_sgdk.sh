#!/bin/bash
#
# setup_sgdk.sh — Install SGDK toolchain for Mega Drive development on macOS
#
# This script:
#   1. Installs m68k-elf-gcc + binutils via Homebrew
#   2. Clones SGDK from GitHub
#   3. Patches SGDK for Homebrew GCC compatibility (disable LTO, replace Java)
#   4. Shows how to set GDK environment variable
#
# Usage: ./setup_sgdk.sh [install_path]
#   Default install path: $HOME/retro-tools/sgdk

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "========================================="
echo "  SGDK Setup for macOS (Homebrew)"
echo "========================================="
echo ""

# Ask for install path
DEFAULT_PATH="$HOME/retro-tools/sgdk"
if [ -n "$1" ]; then
    SGDK_PATH="$1"
else
    read -p "SGDK install path [$DEFAULT_PATH]: " SGDK_PATH
    SGDK_PATH="${SGDK_PATH:-$DEFAULT_PATH}"
fi

echo ""
echo "Install path: $SGDK_PATH"
echo ""

# Step 1: Install Homebrew packages
echo "--- Step 1: Installing m68k-elf toolchain via Homebrew ---"
if ! command -v brew &>/dev/null; then
    echo "ERROR: Homebrew not found. Install from https://brew.sh"
    exit 1
fi

if command -v m68k-elf-gcc &>/dev/null; then
    echo "m68k-elf-gcc already installed: $(which m68k-elf-gcc)"
else
    echo "Installing m68k-elf-binutils and m68k-elf-gcc..."
    brew install m68k-elf-binutils m68k-elf-gcc
fi
echo ""

# Step 2: Clone SGDK
echo "--- Step 2: Cloning SGDK ---"
if [ -d "$SGDK_PATH" ]; then
    echo "SGDK already exists at $SGDK_PATH"
    read -p "Update (git pull)? [y/N]: " UPDATE
    if [ "$UPDATE" = "y" ] || [ "$UPDATE" = "Y" ]; then
        cd "$SGDK_PATH" && git pull
    fi
else
    echo "Cloning SGDK to $SGDK_PATH..."
    mkdir -p "$(dirname "$SGDK_PATH")"
    git clone https://github.com/Stephane-D/SGDK.git "$SGDK_PATH"
fi
echo ""

# Step 3: Patch SGDK
echo "--- Step 3: Patching SGDK for Homebrew GCC ---"

# 3a: Disable LTO in makefile.gen (Homebrew GCC LTO version mismatch)
MAKEFILE_GEN="$SGDK_PATH/makefile.gen"
if grep -q "flto" "$MAKEFILE_GEN"; then
    echo "Disabling LTO in makefile.gen..."
    sed -i '' 's/-fuse-linker-plugin//g' "$MAKEFILE_GEN"
    sed -i '' 's/-flto -flto=auto -ffat-lto-objects//g' "$MAKEFILE_GEN"
    # Add -fno-lto to linker flags
    sed -i '' 's/-Wl,--gc-sections/-Wl,--gc-sections -fno-lto/' "$MAKEFILE_GEN"
    echo "  Done: LTO disabled"
else
    echo "  makefile.gen already patched"
fi

# 3b: Replace Java sizebnd.jar with Python in common.mk
COMMON_MK="$SGDK_PATH/common.mk"
if grep -q 'sizebnd\.jar' "$COMMON_MK"; then
    echo "Replacing Java sizebnd.jar with Python..."
    sed -i '' 's|.*SIZEBND :=.*sizebnd\.jar.*|SIZEBND := python3 \$(BIN)/sizebnd.py|' "$COMMON_MK"
    echo "  Done: Java dependency removed"
else
    echo "  common.mk already patched"
fi

# 3c: Install sizebnd.py
SIZEBND_PY="$SGDK_PATH/bin/sizebnd.py"
if [ ! -f "$SIZEBND_PY" ]; then
    echo "Installing sizebnd.py..."
    cp "$SCRIPT_DIR/sizebnd.py" "$SIZEBND_PY"
    chmod +x "$SIZEBND_PY"
    echo "  Done: sizebnd.py installed"
else
    echo "  sizebnd.py already exists"
fi
echo ""

# Step 4: Verify
echo "--- Step 4: Verification ---"
echo "  m68k-elf-gcc: $(which m68k-elf-gcc 2>/dev/null || echo 'NOT FOUND')"
echo "  SGDK path:    $SGDK_PATH"
echo "  libmd.a:      $(ls "$SGDK_PATH/lib/libmd.a" 2>/dev/null || echo 'NOT FOUND')"
echo "  genesis.h:    $(ls "$SGDK_PATH/inc/genesis.h" 2>/dev/null || echo 'NOT FOUND')"
echo "  sizebnd.py:   $(ls "$SGDK_PATH/bin/sizebnd.py" 2>/dev/null || echo 'NOT FOUND')"
echo ""

# Step 5: Environment setup
echo "========================================="
echo "  Setup Complete!"
echo "========================================="
echo ""
echo "Add this to your shell profile (~/.zshrc or ~/.bashrc):"
echo ""
echo "  export GDK=$SGDK_PATH"
echo ""

# Detect shell and suggest
SHELL_RC=""
if [ -f "$HOME/.zshrc" ]; then
    SHELL_RC="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    SHELL_RC="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile" ]; then
    SHELL_RC="$HOME/.bash_profile"
fi

if [ -n "$SHELL_RC" ]; then
    if grep -q "export GDK=" "$SHELL_RC" 2>/dev/null; then
        echo "GDK is already set in $SHELL_RC"
    else
        read -p "Add 'export GDK=$SGDK_PATH' to $SHELL_RC? [y/N]: " ADD_ENV
        if [ "$ADD_ENV" = "y" ] || [ "$ADD_ENV" = "Y" ]; then
            echo "" >> "$SHELL_RC"
            echo "# SGDK for Mega Drive development" >> "$SHELL_RC"
            echo "export GDK=$SGDK_PATH" >> "$SHELL_RC"
            echo "Added to $SHELL_RC. Run 'source $SHELL_RC' to activate."
        fi
    fi
fi

echo ""
echo "To build the Mega Drive port:"
echo "  cd md && make"
echo ""
