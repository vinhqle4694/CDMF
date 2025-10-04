#include <gtest/gtest.h>
#include "platform/platform_types.h"
#include "platform/dynamic_loader.h"

#ifdef __linux__
#include "platform/linux_loader.h"
#elif defined(_WIN32) || defined(_WIN64)
#include "platform/windows_loader.h"
#endif

#include <memory>
#include <thread>
#include <vector>

using namespace cdmf::platform;

/**
 * @brief Test fixture for platform-specific dynamic loader tests
 */
class DynamicLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef __linux__
        loader = std::make_unique<LinuxLoader>();
        testLibPath = "libtest_module.so";
#elif defined(_WIN32) || defined(_WIN64)
        loader = std::make_unique<WindowsLoader>();
        testLibPath = "test_module.dll";
#endif
    }

    void TearDown() override {
        loader.reset();
    }

    std::unique_ptr<IDynamicLoader> loader;
    std::string testLibPath;
};

// ============================================================================
// Platform Detection Tests
// ============================================================================

TEST(PlatformTypesTest, DetectCurrentPlatform) {
    Platform platform = getCurrentPlatform();
    EXPECT_NE(platform, Platform::UNKNOWN);

#ifdef __linux__
    EXPECT_EQ(platform, Platform::LINUX);
#elif defined(_WIN32) || defined(_WIN64)
    EXPECT_EQ(platform, Platform::WINDOWS);
#elif defined(__APPLE__) && defined(__MACH__)
    EXPECT_EQ(platform, Platform::MACOS);
#endif
}

TEST(PlatformTypesTest, PlatformToString) {
    EXPECT_STREQ(platformToString(Platform::LINUX), "Linux");
    EXPECT_STREQ(platformToString(Platform::WINDOWS), "Windows");
    EXPECT_STREQ(platformToString(Platform::MACOS), "macOS");
    EXPECT_STREQ(platformToString(Platform::UNKNOWN), "Unknown");
}

TEST(PlatformTypesTest, LibraryExtensions) {
    EXPECT_STREQ(getLibraryExtension(Platform::LINUX), ".so");
    EXPECT_STREQ(getLibraryExtension(Platform::WINDOWS), ".dll");
    EXPECT_STREQ(getLibraryExtension(Platform::MACOS), ".dylib");
}

TEST(PlatformTypesTest, LibraryPrefixes) {
    EXPECT_STREQ(getLibraryPrefix(Platform::LINUX), "lib");
    EXPECT_STREQ(getLibraryPrefix(Platform::WINDOWS), "");
    EXPECT_STREQ(getLibraryPrefix(Platform::MACOS), "lib");
}

// ============================================================================
// Dynamic Loader Construction Tests
// ============================================================================

#ifdef __linux__
TEST(LinuxLoaderTest, ConstructorDestructor) {
    EXPECT_NO_THROW({
        LinuxLoader loader;
    });
}

TEST(LinuxLoaderTest, GetPlatform) {
    LinuxLoader loader;
    EXPECT_EQ(loader.getPlatform(), Platform::LINUX);
}
#endif

#ifdef _WIN32
TEST(WindowsLoaderTest, ConstructorDestructor) {
    EXPECT_NO_THROW({
        WindowsLoader loader;
    });
}

TEST(WindowsLoaderTest, GetPlatform) {
    WindowsLoader loader;
    EXPECT_EQ(loader.getPlatform(), Platform::WINDOWS);
}
#endif

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(DynamicLoaderTest, LoadNonexistentLibrary) {
    EXPECT_THROW({
        loader->load("nonexistent_library_12345.so");
    }, DynamicLoaderException);
}

TEST_F(DynamicLoaderTest, LoadEmptyPath) {
    EXPECT_THROW({
        loader->load("");
    }, DynamicLoaderException);
}

TEST_F(DynamicLoaderTest, UnloadInvalidHandle) {
    EXPECT_THROW({
        loader->unload(INVALID_LIBRARY_HANDLE);
    }, DynamicLoaderException);
}

TEST_F(DynamicLoaderTest, UnloadNonexistentHandle) {
    // Create a fake handle (not actually loaded)
    LibraryHandle fakeHandle = reinterpret_cast<LibraryHandle>(0x12345678);
    EXPECT_THROW({
        loader->unload(fakeHandle);
    }, DynamicLoaderException);
}

TEST_F(DynamicLoaderTest, GetSymbolInvalidHandle) {
    EXPECT_THROW({
        loader->getSymbol(INVALID_LIBRARY_HANDLE, "someSymbol");
    }, DynamicLoaderException);
}

TEST_F(DynamicLoaderTest, GetSymbolEmptyName) {
    // Even with invalid handle, should check symbol name first
    LibraryHandle fakeHandle = reinterpret_cast<LibraryHandle>(0x12345678);
    EXPECT_THROW({
        loader->getSymbol(fakeHandle, "");
    }, DynamicLoaderException);
}

// ============================================================================
// System Library Tests (using standard C library)
// ============================================================================

TEST_F(DynamicLoaderTest, LoadSystemLibrary) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";  // Standard C library on Linux
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";  // Standard Windows kernel DLL
#endif

    LibraryHandle handle = INVALID_LIBRARY_HANDLE;
    EXPECT_NO_THROW({
        handle = loader->load(libPath);
    });

    EXPECT_NE(handle, INVALID_LIBRARY_HANDLE);

    // Clean up
    if (handle != INVALID_LIBRARY_HANDLE) {
        EXPECT_NO_THROW({
            loader->unload(handle);
        });
    }
}

TEST_F(DynamicLoaderTest, GetSymbolFromSystemLibrary) {
    std::string libPath;
    std::string symbolName;

#ifdef __linux__
    libPath = "libc.so.6";
    symbolName = "printf";  // Well-known C function
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
    symbolName = "GetCurrentProcessId";  // Well-known Windows API
#endif

    LibraryHandle handle = loader->load(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    void* symbol = nullptr;
    EXPECT_NO_THROW({
        symbol = loader->getSymbol(handle, symbolName);
    });

    EXPECT_NE(symbol, nullptr) << "Symbol '" << symbolName << "' should exist";

    loader->unload(handle);
}

TEST_F(DynamicLoaderTest, GetNonexistentSymbol) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#endif

    LibraryHandle handle = loader->load(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    void* symbol = loader->getSymbol(handle, "nonexistent_symbol_xyz_123");
    EXPECT_EQ(symbol, nullptr) << "Nonexistent symbol should return nullptr";

    loader->unload(handle);
}

TEST_F(DynamicLoaderTest, LoadUnloadMultipleTimes) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#endif

    // Load and unload multiple times
    for (int i = 0; i < 3; ++i) {
        LibraryHandle handle = loader->load(libPath);
        EXPECT_NE(handle, INVALID_LIBRARY_HANDLE);
        loader->unload(handle);
    }
}

TEST_F(DynamicLoaderTest, LoadMultipleLibraries) {
    std::string libPath1, libPath2;

#ifdef __linux__
    libPath1 = "libc.so.6";
    libPath2 = "libm.so.6";  // Math library
#elif defined(_WIN32) || defined(_WIN64)
    libPath1 = "kernel32.dll";
    libPath2 = "user32.dll";
#endif

    LibraryHandle handle1 = loader->load(libPath1);
    LibraryHandle handle2 = loader->load(libPath2);

    EXPECT_NE(handle1, INVALID_LIBRARY_HANDLE);
    EXPECT_NE(handle2, INVALID_LIBRARY_HANDLE);
    EXPECT_NE(handle1, handle2) << "Different libraries should have different handles";

    loader->unload(handle1);
    loader->unload(handle2);
}

// ============================================================================
// Thread Safety Tests (basic)
// ============================================================================

TEST_F(DynamicLoaderTest, ConcurrentLoads) {
    // Load different libraries concurrently to avoid handle conflicts
    std::vector<std::string> libPaths;
#ifdef __linux__
    libPaths = {"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1"};
#elif defined(_WIN32) || defined(_WIN64)
    libPaths = {"kernel32.dll", "user32.dll", "advapi32.dll", "shell32.dll", "ole32.dll"};
#endif

    std::vector<std::thread> threads;
    std::vector<LibraryHandle> handles(libPaths.size(), INVALID_LIBRARY_HANDLE);

    // Launch multiple threads to load different libraries
    for (size_t i = 0; i < libPaths.size(); ++i) {
        threads.emplace_back([this, &libPaths, &handles, i]() {
            handles[i] = loader->load(libPaths[i]);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify all loads succeeded
    for (auto handle : handles) {
        EXPECT_NE(handle, INVALID_LIBRARY_HANDLE);
    }

    // Clean up - unload in reverse order to avoid dependencies
    for (int i = handles.size() - 1; i >= 0; --i) {
        if (handles[i] != INVALID_LIBRARY_HANDLE) {
            EXPECT_NO_THROW(loader->unload(handles[i]));
        }
    }
}

// ============================================================================
// Error Message Tests
// ============================================================================

TEST_F(DynamicLoaderTest, GetLastErrorAfterFailure) {
    try {
        loader->load("nonexistent_library_xyz.so");
    } catch (const DynamicLoaderException&) {
        // Expected exception
    }

    std::string lastError = loader->getLastError();
    EXPECT_FALSE(lastError.empty()) << "Last error should be populated after failure";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
