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
BUILD_TYPE="${BUILD_TYPE:-Release}"
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
RUN_TESTS=1
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
        --no-tests)
            RUN_TESTS=0
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
            echo "  --no-tests             Skip running tests"
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

# Run tests
if [ $RUN_TESTS -eq 1 ]; then
    print_header "Running Tests"

    # Export ENABLE_SOCKET_TESTS for test executables
    export ENABLE_SOCKET_TESTS

    if [ $ENABLE_SOCKET_TESTS -eq 1 ]; then
        print_info "Socket tests: ENABLED"
    else
        print_info "Socket tests: DISABLED (use --enable-socket-tests to enable)"
    fi

    if ctest --output-on-failure; then
        print_success "All tests passed!"
    else
        print_error "Some tests failed"
        exit 1
    fi
else
    print_info "Skipping tests (--no-tests)"
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

# Summary
print_header "Build Summary"
echo -e "${GREEN}✓ Build successful${NC}"
echo -e "  Build directory: $(pwd)"
echo -e "  Libraries: lib/"
echo -e "  Test executables: bin/"

if [ $RUN_TESTS -eq 1 ]; then
    echo -e "${GREEN}✓ All tests passed${NC}"
fi

print_header "Next Steps"
echo "Run individual tests:"
echo ""
echo "PHASE_1: Platform Layer"
echo "  ./bin/test_dynamic_loaders"
echo "  ./bin/test_platform_abstraction"
echo "  ./bin/test_blocking_queue"
echo "  ./bin/test_thread_pool"
echo "  ./bin/test_ring_buffer"
echo ""
echo "PHASE_2: Core Utilities"
echo "  ./bin/test_version"
echo "  ./bin/test_properties"
echo "  ./bin/test_event"
echo ""
echo "PHASE_3: Framework Core"
echo "  ./bin/test_framework_properties"
echo "  ./bin/test_event_dispatcher"
echo ""
echo "PHASE_4: Module Management"
echo "  ./bin/test_module_types"
echo "  ./bin/test_module_handle"
echo "  ./bin/test_manifest_parser"
echo "  ./bin/test_module"
echo "  ./bin/test_module_registry"
echo ""
echo "PHASE_5: Service Layer"
echo "  ./bin/test_service_reference"
echo "  ./bin/test_service_registration"
echo "  ./bin/test_service_registry"
echo "  ./bin/test_service_tracker"
echo ""
echo "PHASE_6: IPC Layer"
echo "  ./bin/test_message"
echo "  ./bin/test_serializers"
echo "  ./bin/test_transports"
echo "  ./bin/test_reliability"
echo "  ./bin/test_connection_management"
echo ""
echo "PHASE_7: Proxy/Stub Generation"
echo "  ./bin/test_proxy_generator"
echo "  ./bin/test_proxy_factory"
echo "  ./bin/test_service_proxy_factory"
echo "  ./bin/test_proxy_stub"
echo ""
echo "PHASE_8: Security Layer"
echo "  ./bin/test_permission"
echo "  ./bin/test_permission_manager"
echo "  ./bin/test_code_verifier"
echo "  ./bin/test_sandbox_manager"
echo "  ./bin/test_resource_limiter"
echo ""
echo "Install libraries:"
echo "  sudo cmake --install . --prefix /usr/local"
