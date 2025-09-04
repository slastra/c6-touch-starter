#!/bin/bash

# ESP32-C6 Touch Starter - Automated Release Script
# Usage: ./scripts/release.sh [patch|minor|major]
# Default: patch increment

set -e  # Exit on any error

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_FILE="$PROJECT_DIR/main/app_config.h"
OTA_REPO_DIR="$PROJECT_DIR/../esp32-ota-firmware"
ESP_IDF_DIR="$PROJECT_DIR/../esp-idf"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse current version from app_config.h
parse_version() {
    grep '#define FIRMWARE_VERSION' "$CONFIG_FILE" | sed 's/.*"v\([0-9]*\.[0-9]*\.[0-9]*\)".*/\1/'
}

# Increment version based on type
increment_version() {
    local version=$1
    local type=${2:-patch}
    
    IFS='.' read -ra VERSION_PARTS <<< "$version"
    local major=${VERSION_PARTS[0]}
    local minor=${VERSION_PARTS[1]}
    local patch=${VERSION_PARTS[2]}
    
    case $type in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
        *)
            log_error "Invalid version type: $type. Use patch, minor, or major."
            exit 1
            ;;
    esac
    
    echo "$major.$minor.$patch"
}

# Update version in app_config.h
update_version() {
    local new_version=$1
    local backup_file="$CONFIG_FILE.backup"
    
    # Create backup
    cp "$CONFIG_FILE" "$backup_file"
    
    # Update version
    sed -i "s/#define FIRMWARE_VERSION.*/#define FIRMWARE_VERSION            \"v$new_version\"/" "$CONFIG_FILE"
    
    log_success "Version updated to v$new_version"
}

# Restore backup on error
restore_backup() {
    if [ -f "$CONFIG_FILE.backup" ]; then
        mv "$CONFIG_FILE.backup" "$CONFIG_FILE"
        log_warning "Restored backup due to error"
    fi
}

# Cleanup function
cleanup() {
    if [ -f "$CONFIG_FILE.backup" ]; then
        rm -f "$CONFIG_FILE.backup"
    fi
}

# Build firmware
build_firmware() {
    log_info "Building firmware..."
    
    cd "$PROJECT_DIR"
    
    # Source ESP-IDF environment
    if [ -f "$ESP_IDF_DIR/export.sh" ]; then
        source "$ESP_IDF_DIR/export.sh"
    else
        log_error "ESP-IDF not found at $ESP_IDF_DIR"
        exit 1
    fi
    
    # Clean and build
    idf.py clean
    idf.py build
    
    # Verify binary exists
    if [ ! -f "build/c6_touch_starter.bin" ]; then
        log_error "Build failed - firmware binary not found"
        exit 1
    fi
    
    log_success "Firmware built successfully"
}

# Create GitHub release
create_release() {
    local version=$1
    local release_notes=$2
    
    log_info "Creating GitHub release v$version..."
    
    # Ensure OTA repository exists and is up to date
    if [ ! -d "$OTA_REPO_DIR" ]; then
        log_info "Cloning OTA repository..."
        cd "$(dirname "$OTA_REPO_DIR")"
        git clone https://github.com/slastra/esp32-ota-firmware.git
    else
        log_info "Updating OTA repository..."
        cd "$OTA_REPO_DIR"
        # Get the default branch name
        local default_branch=$(git symbolic-ref refs/remotes/origin/HEAD | sed 's@^refs/remotes/origin/@@')
        git pull origin "$default_branch"
    fi
    
    # Copy firmware binary
    cp "$PROJECT_DIR/build/c6_touch_starter.bin" "$OTA_REPO_DIR/firmware.bin"
    
    # Create release
    cd "$OTA_REPO_DIR"
    gh release create "v$version" \
        --title "v$version - ESP32-C6 Touch Starter" \
        --notes "$release_notes" \
        firmware.bin
    
    local release_url="https://github.com/slastra/esp32-ota-firmware/releases/tag/v$version"
    log_success "Release created: $release_url"
}

# Generate release notes
generate_release_notes() {
    local old_version=$1
    local new_version=$2
    
    cat << EOF
## ESP32-C6 Touch Starter v$new_version

### Changes
- Version bump from v$old_version to v$new_version
- Updated firmware with latest features and improvements

### Hardware Support
- **Board**: ESP32-C6-Touch-LCD-1.47 (Waveshare)
- **Display**: 172×320 IPS LCD with touch
- **Connectivity**: WiFi 6 + Bluetooth 5

### Features
- ✅ LVGL-based GUI with solid red button styling
- ✅ WiFi connectivity with status display
- ✅ OTA update functionality via touch button
- ✅ Real-time progress tracking
- ✅ Production-ready modular architecture

### Installation
Flash this firmware via OTA by pressing the "Update" button on your device, or manually via ESP-IDF:

\`\`\`bash
idf.py -p /dev/ttyACM0 flash
\`\`\`

🤖 Generated with automated release script
EOF
}

# Main execution
main() {
    local version_type=${1:-patch}
    
    log_info "Starting automated release process..."
    log_info "Version increment type: $version_type"
    
    # Trap errors for cleanup
    trap 'restore_backup; cleanup; log_error "Release process failed"' ERR
    trap 'cleanup' EXIT
    
    # Parse current version
    local current_version=$(parse_version)
    if [ -z "$current_version" ]; then
        log_error "Could not parse current version from $CONFIG_FILE"
        exit 1
    fi
    log_info "Current version: v$current_version"
    
    # Calculate new version
    local new_version=$(increment_version "$current_version" "$version_type")
    log_info "New version: v$new_version"
    
    # Update version in config
    update_version "$new_version"
    
    # Build firmware
    build_firmware
    
    # Generate release notes
    local release_notes=$(generate_release_notes "$current_version" "$new_version")
    
    # Create GitHub release
    create_release "$new_version" "$release_notes"
    
    # Success cleanup
    cleanup
    
    log_success "Release v$new_version created successfully!"
    log_info "The device will automatically download this version on next OTA update."
}

# Check dependencies
check_dependencies() {
    local deps=("gh" "git" "sed")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_info "Please install: ${missing[*]}"
        exit 1
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 [patch|minor|major]"
    echo ""
    echo "Version increment types:"
    echo "  patch  - Increment patch version (x.x.X) [default]"
    echo "  minor  - Increment minor version (x.X.0)"
    echo "  major  - Increment major version (X.0.0)"
    echo ""
    echo "Examples:"
    echo "  $0        # Increment patch: 2.4.0 -> 2.4.1"
    echo "  $0 minor  # Increment minor: 2.4.0 -> 2.5.0"
    echo "  $0 major  # Increment major: 2.4.0 -> 3.0.0"
}

# Handle help flag
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_usage
    exit 0
fi

# Validate arguments
if [ $# -gt 1 ]; then
    log_error "Too many arguments"
    show_usage
    exit 1
fi

if [ $# -eq 1 ] && [[ ! "$1" =~ ^(patch|minor|major)$ ]]; then
    log_error "Invalid version type: $1"
    show_usage
    exit 1
fi

# Run checks and main process
check_dependencies
main "$@"