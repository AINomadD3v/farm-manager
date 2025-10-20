#!/bin/bash
# =============================================================================
# Hardware Acceleration Build and Test Script
# =============================================================================

set -e  # Exit on error

echo "=============================================="
echo "Hardware Acceleration Build and Test Script"
echo "=============================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# =============================================================================
# Step 1: Check Prerequisites
# =============================================================================
echo "Step 1: Checking prerequisites..."

# Check FFmpeg
if command -v ffmpeg &> /dev/null; then
    echo -e "${GREEN}✓${NC} FFmpeg found: $(ffmpeg -version | head -n1)"

    # Check hardware acceleration support
    echo ""
    echo "Hardware acceleration support:"
    ffmpeg -hwaccels 2>/dev/null | grep -E "vaapi|qsv|cuda" && \
        echo -e "${GREEN}✓${NC} Hardware decoders available" || \
        echo -e "${YELLOW}!${NC} No hardware decoders found (will use software fallback)"
else
    echo -e "${RED}✗${NC} FFmpeg not found!"
    exit 1
fi

# Check Qt
if command -v qmake &> /dev/null; then
    echo -e "${GREEN}✓${NC} Qt found: $(qmake -v | grep "Qt version" | head -n1)"
else
    echo -e "${YELLOW}!${NC} qmake not found (might be in nix environment)"
fi

# Check OpenGL
if command -v glxinfo &> /dev/null; then
    GL_VERSION=$(glxinfo | grep "OpenGL version" | head -n1)
    echo -e "${GREEN}✓${NC} OpenGL: $GL_VERSION"
else
    echo -e "${YELLOW}!${NC} glxinfo not found, can't check OpenGL version"
fi

echo ""

# =============================================================================
# Step 2: Build
# =============================================================================
echo "Step 2: Building project..."
cd "$(dirname "$0")/QtScrcpy"

# Create build directory
mkdir -p build
cd build

# Configure
echo "Running CMake..."
cmake .. || {
    echo -e "${RED}✗${NC} CMake configuration failed!"
    exit 1
}

# Build
echo "Building..."
make -j$(nproc) || {
    echo -e "${RED}✗${NC} Build failed!"
    exit 1
}

echo -e "${GREEN}✓${NC} Build successful!"
echo ""

# =============================================================================
# Step 3: Check for hardware decoder libraries
# =============================================================================
echo "Step 3: Checking FFmpeg hardware decoder support..."

# Check linked libraries
if ldd ./QtScrcpy | grep -q libavcodec; then
    echo -e "${GREEN}✓${NC} FFmpeg libraries linked"

    # Check for hardware support in libraries
    if strings $(ldd ./QtScrcpy | grep libavcodec | awk '{print $3}') | grep -q "h264_vaapi"; then
        echo -e "${GREEN}✓${NC} VAAPI decoder found in libavcodec"
    fi

    if strings $(ldd ./QtScrcpy | grep libavcodec | awk '{print $3}') | grep -q "h264_qsv"; then
        echo -e "${GREEN}✓${NC} QSV decoder found in libavcodec"
    fi

    if strings $(ldd ./QtScrcpy | grep libavcodec | awk '{print $3}') | grep -q "h264_cuvid"; then
        echo -e "${GREEN}✓${NC} CUDA decoder found in libavcodec"
    fi
else
    echo -e "${YELLOW}!${NC} Could not verify FFmpeg linking"
fi

echo ""

# =============================================================================
# Step 4: Provide test instructions
# =============================================================================
echo "=============================================="
echo "Build Complete!"
echo "=============================================="
echo ""
echo "To test hardware acceleration:"
echo ""
echo "1. Check available hardware decoders:"
echo "   ffmpeg -decoders | grep h264"
echo ""
echo "2. Run the application:"
echo "   ./QtScrcpy"
echo ""
echo "3. Check logs for hardware decoder initialization:"
echo "   Look for: 'Successfully opened hardware decoder: h264_XXX'"
echo "   If you see: 'Using software decoder (CPU fallback)' - hardware didn't work"
echo ""
echo "4. Monitor performance:"
echo "   # CPU usage (should be ~2% per device with hardware)"
echo "   top -p \$(pgrep QtScrcpy)"
echo ""
echo "   # GPU usage - Intel"
echo "   intel_gpu_top"
echo ""
echo "   # GPU usage - NVIDIA"
echo "   nvidia-smi dmon"
echo ""
echo "5. Connect devices and test:"
echo "   - Click 'Farm Viewer' button"
echo "   - Auto-detect should find all devices"
echo "   - CPU usage should scale linearly (~2% per device)"
echo "   - GPU usage should be 60-80% with 100 devices"
echo ""
echo "Expected performance (per device):"
echo "  Hardware: 2-4% CPU, 10-20% GPU"
echo "  Software: 20-36% CPU, 0% GPU"
echo ""
echo "For 100 devices:"
echo "  Hardware: 200-400% CPU ✓"
echo "  Software: 2000%+ CPU ✗ (impossible)"
echo ""
echo "Documentation:"
echo "  - HARDWARE_ACCELERATION_GUIDE.md - Complete guide"
echo "  - IMPLEMENTATION_SUMMARY.md - Technical details"
echo "  - OPENGL_SHADERS.glsl - Shader reference"
echo ""
echo "=============================================="

# =============================================================================
# Step 5: Optional - Check GPU info
# =============================================================================
if command -v lspci &> /dev/null; then
    echo ""
    echo "Detected GPUs:"
    lspci | grep -i vga
    lspci | grep -i 3d
fi

# Check VAAPI support
if [ -e /dev/dri/renderD128 ]; then
    echo ""
    echo -e "${GREEN}✓${NC} VAAPI device found: /dev/dri/renderD128"
    ls -l /dev/dri/renderD*

    if command -v vainfo &> /dev/null; then
        echo ""
        echo "VAAPI capabilities:"
        vainfo 2>/dev/null | grep -A 10 "VAProfile" || true
    fi
fi

# Check NVIDIA
if command -v nvidia-smi &> /dev/null; then
    echo ""
    echo "NVIDIA GPU info:"
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader
fi

echo ""
echo "Build script complete!"
