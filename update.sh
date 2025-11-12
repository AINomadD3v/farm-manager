#!/usr/bin/env bash
# QtScrcpy Phone Farm Manager - Update and Rebuild Script
# Automatically updates from git, rebuilds, and optionally reinstalls

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# Configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
AUTO_INSTALL="${AUTO_INSTALL:-false}"
SHOW_CHANGES="${SHOW_CHANGES:-true}"

echo -e "${BOLD}${CYAN}=========================================================="
echo "QtScrcpy Phone Farm Manager - Update & Rebuild"
echo "==========================================================${NC}"
echo ""

# Function to display help
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -i, --install           Automatically install after rebuild"
    echo "  -s, --skip-changes      Skip showing git changes"
    echo "  -d, --debug             Build in Debug mode instead of Release"
    echo "  -c, --clean             Force clean rebuild (removes build cache)"
    echo "  --no-pull              Skip git pull (rebuild only)"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_TYPE=Debug|Release    Build type (default: Release)"
    echo "  AUTO_INSTALL=true|false     Auto install after build (default: false)"
    echo ""
    echo "Examples:"
    echo "  $0                      # Update, rebuild, prompt for install"
    echo "  $0 --install            # Update, rebuild, and auto-install"
    echo "  $0 --no-pull            # Just rebuild without updating"
    echo "  $0 --clean              # Force clean rebuild (ensures all fixes applied)"
    echo ""
    exit 0
}

# Parse command line arguments
SKIP_PULL=false
FORCE_CLEAN_BUILD=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            ;;
        -i|--install)
            AUTO_INSTALL=true
            shift
            ;;
        -s|--skip-changes)
            SHOW_CHANGES=false
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            FORCE_CLEAN_BUILD=true
            shift
            ;;
        --no-pull)
            SKIP_PULL=true
            shift
            ;;
        *)
            echo -e "${RED}Error: Unknown option: $1${NC}"
            echo "Run '$0 --help' for usage information"
            exit 1
            ;;
    esac
done

# Check if we're in a git repository
echo -e "${BLUE}Step 1: Verifying git repository...${NC}"
if [ ! -d ".git" ]; then
    echo -e "${RED}Error: Not a git repository${NC}"
    echo "This script must be run from the farm-manager git repository"
    exit 1
fi
echo -e "${GREEN}✓ Git repository verified${NC}"
echo ""

# Show current version
echo -e "${BLUE}Current version:${NC}"
CURRENT_COMMIT=$(git rev-parse --short HEAD)
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
CURRENT_DATE=$(git log -1 --format=%cd --date=short)
echo -e "  Branch: ${CYAN}${CURRENT_BRANCH}${NC}"
echo -e "  Commit: ${CYAN}${CURRENT_COMMIT}${NC} (${CURRENT_DATE})"
echo -e "  Message: $(git log -1 --format=%s)"
echo ""

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${YELLOW}⚠ Warning: You have uncommitted changes${NC}"
    git status --short
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Update cancelled"
        exit 1
    fi
    echo ""
fi

# Pull latest changes (unless --no-pull)
if [ "$SKIP_PULL" = false ]; then
    echo -e "${BLUE}Step 2: Fetching latest changes from git...${NC}"
    git fetch origin

    # Check if we're behind
    COMMITS_BEHIND=$(git rev-list --count HEAD..origin/${CURRENT_BRANCH})

    if [ "$COMMITS_BEHIND" -eq 0 ]; then
        echo -e "${GREEN}✓ Already up to date (no changes)${NC}"
        echo ""

        read -p "No updates available. Rebuild anyway? (Y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            echo "Cancelled"
            exit 0
        fi
    else
        echo -e "${YELLOW}${COMMITS_BEHIND} new commit(s) available${NC}"
        echo ""

        if [ "$SHOW_CHANGES" = true ]; then
            echo -e "${BLUE}Changes since your version:${NC}"
            git log --oneline --decorate HEAD..origin/${CURRENT_BRANCH} | head -20
            echo ""

            echo -e "${BLUE}Files changed:${NC}"
            git diff --stat HEAD..origin/${CURRENT_BRANCH}
            echo ""
        fi

        read -p "Pull and update? (Y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            echo "Update cancelled"
            exit 0
        fi

        echo ""
        echo -e "${BLUE}Pulling changes...${NC}"
        git pull origin ${CURRENT_BRANCH}

        NEW_COMMIT=$(git rev-parse --short HEAD)
        echo -e "${GREEN}✓ Updated from ${CURRENT_COMMIT} to ${NEW_COMMIT}${NC}"
        echo ""
    fi
else
    echo -e "${YELLOW}Step 2: Skipping git pull (--no-pull specified)${NC}"
    echo ""
fi

# Enter nix development environment and rebuild
echo -e "${BLUE}Step 3: Rebuilding application (${BUILD_TYPE} mode)...${NC}"
echo ""

# Check if we need to enter nix develop
if [ -f "scripts/farm-functions.sh" ]; then
    echo -e "${CYAN}Loading build environment...${NC}"
    source scripts/farm-functions.sh

    # Force clean rebuild if requested or if we pulled new changes (to ensure all fixes are compiled)
    if [ "$FORCE_CLEAN_BUILD" = true ]; then
        echo -e "${CYAN}Clean rebuild requested - ensuring all fixes are applied${NC}"
        export FORCE_CLEAN_BUILD=true
    elif [ "$SKIP_PULL" = false ] && [ "${COMMITS_BEHIND:-0}" -gt 0 ]; then
        echo -e "${CYAN}New changes detected - forcing clean rebuild to ensure all fixes are applied${NC}"
        export FORCE_CLEAN_BUILD=true
    fi

    # Build based on BUILD_TYPE
    if [ "$BUILD_TYPE" = "Debug" ]; then
        build_debug
    else
        build_release
    fi

    BUILD_EXIT_CODE=$?
else
    echo -e "${RED}Error: scripts/farm-functions.sh not found${NC}"
    echo "Cannot load build functions"
    exit 1
fi

if [ $BUILD_EXIT_CODE -ne 0 ]; then
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}✓ Build completed successfully${NC}"
echo ""

# Verify binary exists
BINARY_PATH="./output/x64/Release/QtScrcpy"
if [ "$BUILD_TYPE" = "Debug" ]; then
    BINARY_PATH="./output/x64/Debug/QtScrcpy"
fi

if [ ! -f "${BINARY_PATH}" ]; then
    echo -e "${RED}Error: Binary not found at ${BINARY_PATH}${NC}"
    exit 1
fi

BINARY_SIZE=$(du -h "${BINARY_PATH}" | cut -f1)
echo -e "${BLUE}Binary information:${NC}"
echo -e "  Location: ${BINARY_PATH}"
echo -e "  Size: ${BINARY_SIZE}"
echo ""

# Check if installed and offer to update installation
INSTALLED_BINARY="${HOME}/.local/share/qtscrcpy-farm/QtScrcpy"
IS_INSTALLED=false

if [ -f "${INSTALLED_BINARY}" ]; then
    IS_INSTALLED=true
    INSTALLED_SIZE=$(du -h "${INSTALLED_BINARY}" | cut -f1)
    echo -e "${BLUE}Installation detected:${NC}"
    echo -e "  Installed binary: ${INSTALLED_BINARY}"
    echo -e "  Installed size: ${INSTALLED_SIZE}"
    echo ""
fi

# Install or update
if [ "$AUTO_INSTALL" = true ] || [ "$IS_INSTALLED" = true ]; then
    if [ "$AUTO_INSTALL" = false ]; then
        if [ "$IS_INSTALLED" = true ]; then
            read -p "Update installation? (Y/n) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Nn]$ ]]; then
                echo -e "${YELLOW}Installation skipped${NC}"
                SKIP_INSTALL=true
            else
                SKIP_INSTALL=false
            fi
        fi
    else
        SKIP_INSTALL=false
    fi

    if [ "${SKIP_INSTALL:-false}" = false ]; then
        echo ""
        echo -e "${BLUE}Step 4: Installing/Updating application...${NC}"

        # Detect NixOS
        if [ -f "/etc/os-release" ] && grep -q "NixOS" /etc/os-release; then
            INSTALL_SCRIPT="./install-nixos.sh"
        else
            INSTALL_SCRIPT="./install.sh"
        fi

        if [ -f "${INSTALL_SCRIPT}" ]; then
            bash "${INSTALL_SCRIPT}"
        else
            echo -e "${RED}Error: Install script not found: ${INSTALL_SCRIPT}${NC}"
            exit 1
        fi
    fi
else
    echo -e "${YELLOW}Installation not detected and auto-install not requested${NC}"
    echo ""
    echo "To install, run one of:"
    echo -e "  ${CYAN}./install-nixos.sh${NC}  (for NixOS)"
    echo -e "  ${CYAN}./install.sh${NC}       (for other Linux)"
    echo ""
fi

echo ""
echo -e "${BOLD}${GREEN}=========================================================="
echo "Update Complete!"
echo "==========================================================${NC}"
echo ""

if [ "$IS_INSTALLED" = true ] || [ "${SKIP_INSTALL:-false}" = false ]; then
    echo -e "${BLUE}You can now run the application:${NC}"
    echo -e "  ${CYAN}qtscrcpy-farm${NC}                    # From anywhere"
    echo -e "  ${CYAN}${BINARY_PATH}${NC}   # From source directory"
else
    echo -e "${BLUE}You can run the application from source:${NC}"
    echo -e "  ${CYAN}${BINARY_PATH}${NC}"
    echo ""
    echo -e "${YELLOW}To install system-wide, run:${NC}"
    if [ -f "/etc/os-release" ] && grep -q "NixOS" /etc/os-release; then
        echo -e "  ${CYAN}./install-nixos.sh${NC}"
    else
        echo -e "  ${CYAN}./install.sh${NC}"
    fi
fi

echo ""
echo -e "${BLUE}Build information:${NC}"
echo -e "  Build type: ${BUILD_TYPE}"
echo -e "  Git commit: $(git rev-parse --short HEAD)"
echo -e "  Build date: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""
