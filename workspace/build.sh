#!/bin/bash

# CDMF Build Script for Linux/macOS
# Builds PHASE_1 libraries and runs tests

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

# Parse arguments
CLEAN=0
RUN_TESTS=0
RUN_AUTO_TESTS=0
RUN_CDMF=0
ENABLE_COVERAGE=0
ENABLE_SANITIZERS=0
SHARED_LIBS=ON
ENABLE_SOCKET_TESTS="${ENABLE_SOCKET_TESTS:-1}"

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN=1
            shift
            ;;
        --tests)
            RUN_TESTS=1
            shift
            ;;
        --auto-test)
            RUN_AUTO_TESTS=1
            shift
            ;;
        --run)
            RUN_CDMF=1
            RUN_TESTS=0
            RUN_AUTO_TESTS=0
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE=1
            BUILD_TYPE="Debug"
            shift
            ;;
        --sanitizers)
            ENABLE_SANITIZERS=1
            BUILD_TYPE="Debug"
            shift
            ;;
        --static)
            SHARED_LIBS=OFF
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --enable-socket-tests)
            ENABLE_SOCKET_TESTS=1
            shift
            ;;
        -j*)
            NUM_JOBS="${1#-j}"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean                Remove build directory before building"
            echo "  --tests                Run unit tests after building (disabled by default)"
            echo "  --auto-test            Run automation tests after building (disabled by default)"
            echo "  --run                  Run CDMF executable only (skip build and tests)"
            echo "  --coverage             Enable code coverage (implies --debug)"
            echo "  --sanitizers           Enable sanitizers (implies --debug)"
            echo "  --static               Build static libraries"
            echo "  --debug                Debug build"
            echo "  --release              Release build (default)"
            echo "  --enable-socket-tests  Enable socket tests (requires Unix sockets)"
            echo "  -jN                    Use N parallel jobs (default: CPU count)"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Skip build if only running
if [ $RUN_CDMF -eq 1 ]; then
    print_info "Skipping build (--run only mode)"
    cd "$BUILD_DIR"
else
    # Clean build directory if requested
    if [ $CLEAN -eq 1 ]; then
        print_info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    fi

    # Create build directory
    print_info "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Configure
    print_header "Configuring CDMF PHASE_1"
    print_info "Build type: $BUILD_TYPE"
    print_info "Parallel jobs: $NUM_JOBS"
    print_info "Shared libraries: $SHARED_LIBS"

    CMAKE_ARGS=(
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCDMF_BUILD_SHARED_LIBS="$SHARED_LIBS"
        -DCDMF_BUILD_TESTS=ON
    )

    if [ $ENABLE_COVERAGE -eq 1 ]; then
        print_info "Code coverage: ENABLED"
        CMAKE_ARGS+=(-DCDMF_ENABLE_COVERAGE=ON)
    fi

    if [ $ENABLE_SANITIZERS -eq 1 ]; then
        print_info "Sanitizers: ENABLED"
        CMAKE_ARGS+=(-DCDMF_ENABLE_SANITIZERS=ON)
    fi

    cmake .. "${CMAKE_ARGS[@]}"
    print_success "Configuration complete"

    # Build
    print_header "Building CDMF PHASE_1"
    cmake --build . -j"$NUM_JOBS"
    print_success "Build complete"

    # List built libraries
    print_header "Built Libraries"
    ls -lh lib/ 2>/dev/null || true

    print_header "Built Test Executables"
    ls -lh bin/ 2>/dev/null || true
fi

# Run unit tests
if [ $RUN_TESTS -eq 1 ]; then
    print_header "Running Unit Tests"

    # Export ENABLE_SOCKET_TESTS for test executables
    export ENABLE_SOCKET_TESTS

    if [ $ENABLE_SOCKET_TESTS -eq 1 ]; then
        print_info "Socket tests: ENABLED"
    else
        print_info "Socket tests: DISABLED (use --enable-socket-tests to enable)"
    fi

    # Run only unit tests (exclude automation tests)
    if ctest --output-on-failure -E "AutomationSystemTest"; then
        print_success "All unit tests passed!"
    else
        print_error "Some unit tests failed"
        exit 1
    fi
else
    print_info "Skipping unit tests (use --tests to run unit tests)"
fi

# Run automation tests
if [ $RUN_AUTO_TESTS -eq 1 ]; then
    print_header "Running Automation Tests"

    # Set environment variables for automation tests
    export CDMF_TEST_EXECUTABLE="$(pwd)/bin/cdmf"
    export CDMF_TEST_CONFIG="$(pwd)/config/framework.json"
    export CDMF_TEST_LOG_FILE="$(pwd)/tests/automation_test/logs/test_automation.log"
    export CDMF_TEST_WORKING_DIR="$(pwd)"

    print_info "Test Configuration:"
    print_info "  Executable: $CDMF_TEST_EXECUTABLE"
    print_info "  Config: $CDMF_TEST_CONFIG"
    print_info "  Log file: $CDMF_TEST_LOG_FILE"
    print_info "  Working dir: $CDMF_TEST_WORKING_DIR"

    # Run only automation tests
    if ctest -R "AutomationSystemTest" --output-on-failure; then
        print_success "All automation tests passed!"
    else
        print_error "Some automation tests failed"
        exit 1
    fi
else
    print_info "Skipping automation tests (use --auto-test to run automation tests)"
fi

# Generate coverage report
if [ $ENABLE_COVERAGE -eq 1 ]; then
    print_header "Generating Coverage Report"

    if command -v lcov &> /dev/null; then
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/googletest/*' --output-file coverage.info
        lcov --list coverage.info

        if command -v genhtml &> /dev/null; then
            genhtml coverage.info --output-directory coverage_html
            print_success "Coverage report generated: $(pwd)/coverage_html/index.html"
        else
            print_info "Install genhtml to generate HTML coverage report"
        fi
    else
        print_info "Install lcov to generate coverage reports"
    fi
fi

# Run CDMF executable
if [ $RUN_CDMF -eq 1 ]; then
    print_header "Running CDMF"

    # Set environment variable for framework config
    export CDMF_FRAMEWORK_CONFIG="$(pwd)/config/framework.json"
    print_info "CDMF_FRAMEWORK_CONFIG=$CDMF_FRAMEWORK_CONFIG"

    # Check if executable exists
    if [ -f "bin/cdmf" ]; then
        cd ..
        ./build/bin/cdmf
    else
        print_error "CDMF executable not found at bin/cdmf"
        exit 1
    fi
fi

# Summary
if [ $RUN_CDMF -eq 0 ]; then
    print_header "Build Summary"
    echo -e "${GREEN}✓ Build successful${NC}"
    echo -e "  Build directory: $(pwd)"
    echo -e "  Libraries: lib/"
    echo -e "  Test executables: bin/"

    if [ $RUN_TESTS -eq 1 ]; then
        echo -e "${GREEN}✓ All unit tests passed${NC}"
    fi

    if [ $RUN_AUTO_TESTS -eq 1 ]; then
        echo -e "${GREEN}✓ All automation tests passed${NC}"
    fi

    echo "Install libraries:"
    echo "  sudo cmake --install . --prefix /usr/local"
fi
