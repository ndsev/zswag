#!/bin/bash

# fetch_dependencies.sh
# Script to prefetch all FetchContent dependencies for offline builds
# Automatically reads dependency information from cmake/Dependencies.cmake
#
# Usage: ./fetch_dependencies.sh [build_directory]
#   build_directory: Optional path to build directory (default: _deps in project root)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory (should contain CMakeLists.txt)
if [ ! -f "CMakeLists.txt" ]; then
    print_error "This script should be run from the project root directory (where CMakeLists.txt is located)"
    exit 1
fi

# Check if Dependencies.cmake exists
if [ ! -f "cmake/Dependencies.cmake" ]; then
    print_error "cmake/Dependencies.cmake file not found"
    exit 1
fi

# Function to parse dependencies from Dependencies.cmake
parse_dependencies() {
    local deps_file="cmake/Dependencies.cmake"
    local temp_file=$(mktemp)
    
    # Remove comments and empty lines, then extract FetchContent_Declare blocks
    grep -v "^#" "$deps_file" | grep -v "^$" > "$temp_file"
    
    # Parse each FetchContent_Declare block
    awk '
    /FetchContent_Declare\(/ {
        in_declare = 1
        name = ""
        repo = ""
        tag = ""
        next
    }
    
    in_declare == 1 {
        # Extract dependency name (first non-empty line after FetchContent_Declare)
        if (name == "" && $1 != "" && $1 !~ /^#/) {
            name = $1
        }
        
        # Extract GIT_REPOSITORY
        if ($1 == "GIT_REPOSITORY") {
            repo = $2
        }
        
        # Extract GIT_TAG
        if ($1 == "GIT_TAG") {
            tag = $2
        }
        
        # End of declaration
        if ($0 ~ /^\)/) {
            if (name != "" && repo != "" && tag != "") {
                print name "|" repo "|" tag
            }
            in_declare = 0
        }
    }
    ' "$temp_file"
    
    rm -f "$temp_file"
}

# Function to fetch a single dependency
fetch_dependency() {
    local name="$1"
    local repo="$2"
    local tag="$3"
    
    print_status "Fetching $name from $repo (tag: $tag)..."
    
    # Create CMakeLists.txt for this dependency
    cat > CMakeLists.txt << EOF
cmake_minimum_required(VERSION 3.22.3)
project(fetch_${name})

include(FetchContent)

FetchContent_Declare(
    ${name}
    GIT_REPOSITORY ${repo}
    GIT_TAG        ${tag}
)

FetchContent_MakeAvailable(${name})
EOF

    # Run cmake to fetch the dependency
    if cmake . -DFETCHCONTENT_QUIET=OFF > /dev/null 2>&1; then
        print_success "Successfully fetched $name"
        return 0
    else
        print_error "Failed to fetch $name"
        return 1
    fi
}

# Parse command line arguments
BUILD_DIR=""
if [ $# -gt 0 ]; then
    BUILD_DIR="$1"
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory '$BUILD_DIR' does not exist"
        exit 1
    fi
fi

# Create a temporary build directory for fetching dependencies
TEMP_BUILD_DIR="temp_fetch_build"
DEPS_DIR="_deps"

# Determine target directory for dependencies
if [ -n "$BUILD_DIR" ]; then
    TARGET_DEPS_DIR="$BUILD_DIR/_deps"
    print_status "Will fetch dependencies to: $TARGET_DEPS_DIR"
else
    TARGET_DEPS_DIR="$DEPS_DIR"
    print_status "Will fetch dependencies to: $TARGET_DEPS_DIR (project root)"
fi

print_status "Parsing dependencies from cmake/Dependencies.cmake..."

# Parse dependencies from the CMake file
dependencies=$(parse_dependencies)

if [ -z "$dependencies" ]; then
    print_error "No dependencies found in cmake/Dependencies.cmake"
    exit 1
fi

# Count dependencies
dep_count=$(echo "$dependencies" | wc -l)
print_status "Found $dep_count dependencies to fetch"

print_status "Creating temporary build directory: $TEMP_BUILD_DIR"
mkdir -p "$TEMP_BUILD_DIR"
cd "$TEMP_BUILD_DIR"

print_status "Starting dependency prefetch process..."

# Fetch each dependency
failed_count=0
success_count=0

while IFS='|' read -r name repo tag; do
    if [ -n "$name" ] && [ -n "$repo" ] && [ -n "$tag" ]; then
        if fetch_dependency "$name" "$repo" "$tag"; then
            ((success_count++))
        else
            ((failed_count++))
        fi
        
        # Clean up for next dependency
        rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake Makefile
    fi
done <<< "$dependencies"

# Go back to project root
cd ..

# Move the _deps directory to the target location if it exists
if [ -d "$TEMP_BUILD_DIR/$DEPS_DIR" ]; then
    print_status "Moving fetched dependencies to target location..."
    if [ -d "$TARGET_DEPS_DIR" ]; then
        print_warning "Existing $TARGET_DEPS_DIR directory found. Backing up to ${TARGET_DEPS_DIR}.backup"
        mv "$TARGET_DEPS_DIR" "${TARGET_DEPS_DIR}.backup"
    fi
    
    # Create parent directory if it doesn't exist
    mkdir -p "$(dirname "$TARGET_DEPS_DIR")"
    mv "$TEMP_BUILD_DIR/$DEPS_DIR" "$TARGET_DEPS_DIR"
    print_success "Dependencies moved to $TARGET_DEPS_DIR/"
fi

# Clean up temporary build directory
print_status "Cleaning up temporary build directory..."
rm -rf "$TEMP_BUILD_DIR"

# Print summary
echo ""
if [ $failed_count -eq 0 ]; then
    print_success "All $success_count dependencies have been prefetched successfully!"
else
    print_warning "Prefetch completed with $success_count successes and $failed_count failures"
fi

print_status "You can now build offline using: cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON ..."

# Print usage information
echo ""
if [ -n "$BUILD_DIR" ]; then
    echo "Usage for offline builds (dependencies are in build directory):"
    echo "  cd $BUILD_DIR"
    echo "  cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON .."
    echo "  make"
else
    echo "Usage for offline builds:"
    echo "  mkdir build && cd build"
    echo "  cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON .."
    echo "  make"
    echo ""
    echo "For better offline builds, prefetch to build directory:"
    echo "  mkdir build"
    echo "  ./fetch_dependencies.sh build"
    echo "  cd build && cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON .."
fi
echo ""
echo "Usage with local overrides:"
echo "  cmake -DFETCHCONTENT_SOURCE_DIR_SPDLOG=/path/to/local/spdlog .."
echo ""
echo "Note: This script automatically reads dependencies from cmake/Dependencies.cmake"
echo "      No need to manually update dependency lists when adding new dependencies!" 