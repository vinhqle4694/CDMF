#include <gtest/gtest.h>
#include "platform/platform_abstraction.h"
#include <thread>
#include <vector>

using namespace cdmf::platform;

/**
 * @brief Test fixture for PlatformAbstraction tests
 */
class PlatformAbstractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        platform = std::make_unique<PlatformAbstraction>();
    }

    void TearDown() override {
        platform.reset();
    }

    std::unique_ptr<PlatformAbstraction> platform;
};

// ============================================================================
// Construction and Initialization Tests
// ============================================================================

TEST_F(PlatformAbstractionTest, ConstructorInitialization) {
    EXPECT_NO_THROW({
        PlatformAbstraction p;
    });
}

TEST_F(PlatformAbstractionTest, PlatformDetection) {
    Platform detected = platform->getPlatform();
    EXPECT_NE(detected, Platform::UNKNOWN);

#ifdef __linux__
    EXPECT_EQ(detected, Platform::LINUX);
#elif defined(_WIN32) || defined(_WIN64)
    EXPECT_EQ(detected, Platform::WINDOWS);
#elif defined(__APPLE__)
    EXPECT_EQ(detected, Platform::MACOS);
#endif
}

TEST_F(PlatformAbstractionTest, LibraryExtension) {
    const char* ext = platform->getLibraryExtension();
    ASSERT_NE(ext, nullptr);

#ifdef __linux__
    EXPECT_STREQ(ext, ".so");
#elif defined(_WIN32) || defined(_WIN64)
    EXPECT_STREQ(ext, ".dll");
#elif defined(__APPLE__)
    EXPECT_STREQ(ext, ".dylib");
#endif
}

TEST_F(PlatformAbstractionTest, LibraryPrefix) {
    const char* prefix = platform->getLibraryPrefix();
    ASSERT_NE(prefix, nullptr);

#if defined(__linux__) || defined(__APPLE__)
    EXPECT_STREQ(prefix, "lib");
#elif defined(_WIN32) || defined(_WIN64)
    EXPECT_STREQ(prefix, "");
#endif
}

// ============================================================================
// Library Loading Tests
// ============================================================================

TEST_F(PlatformAbstractionTest, LoadSystemLibrary) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
#endif

    LibraryHandle handle = INVALID_LIBRARY_HANDLE;
    EXPECT_NO_THROW({
        handle = platform->loadLibrary(libPath);
    });

    EXPECT_NE(handle, INVALID_LIBRARY_HANDLE);
    EXPECT_TRUE(platform->isLibraryLoaded(handle));
    EXPECT_EQ(platform->getLoadedLibraryCount(), 1);

    platform->unloadLibrary(handle);
}

TEST_F(PlatformAbstractionTest, LoadNonexistentLibrary) {
    EXPECT_THROW({
        platform->loadLibrary("nonexistent_library_xyz.so");
    }, DynamicLoaderException);
}

TEST_F(PlatformAbstractionTest, UnloadLibrary) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    EXPECT_NO_THROW({
        platform->unloadLibrary(handle);
    });

    EXPECT_FALSE(platform->isLibraryLoaded(handle));
    EXPECT_EQ(platform->getLoadedLibraryCount(), 0);
}

TEST_F(PlatformAbstractionTest, UnloadInvalidHandle) {
    EXPECT_THROW({
        platform->unloadLibrary(INVALID_LIBRARY_HANDLE);
    }, DynamicLoaderException);
}

// ============================================================================
// Symbol Resolution Tests
// ============================================================================

TEST_F(PlatformAbstractionTest, GetSymbol) {
    std::string libPath, symbolName;
#ifdef __linux__
    libPath = "libc.so.6";
    symbolName = "printf";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
    symbolName = "GetCurrentProcessId";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
    symbolName = "printf";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    void* symbol = platform->getSymbol(handle, symbolName);
    EXPECT_NE(symbol, nullptr);

    platform->unloadLibrary(handle);
}

TEST_F(PlatformAbstractionTest, GetTypedSymbol) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

#ifdef __linux__
    // printf has signature: int printf(const char*, ...)
    using PrintfFn = int (*)(const char*, ...);
    auto printfFn = platform->getSymbol<PrintfFn>(handle, "printf");
    EXPECT_NE(printfFn, nullptr);
#elif defined(_WIN32) || defined(_WIN64)
    // GetCurrentProcessId has signature: DWORD WINAPI GetCurrentProcessId(void)
    using GetPidFn = unsigned long (*)();
    auto getPidFn = platform->getSymbol<GetPidFn>(handle, "GetCurrentProcessId");
    EXPECT_NE(getPidFn, nullptr);
#endif

    platform->unloadLibrary(handle);
}

TEST_F(PlatformAbstractionTest, GetNonexistentSymbol) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    void* symbol = platform->getSymbol(handle, "nonexistent_function_xyz");
    EXPECT_EQ(symbol, nullptr);

    platform->unloadLibrary(handle);
}

TEST_F(PlatformAbstractionTest, GetSymbolFromInvalidHandle) {
    EXPECT_THROW({
        platform->getSymbol(INVALID_LIBRARY_HANDLE, "someSymbol");
    }, DynamicLoaderException);
}

// ============================================================================
// Library Tracking Tests
// ============================================================================

TEST_F(PlatformAbstractionTest, GetLibraryPath) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    std::string retrievedPath = platform->getLibraryPath(handle);
    EXPECT_EQ(retrievedPath, libPath);

    platform->unloadLibrary(handle);
}

TEST_F(PlatformAbstractionTest, GetLibraryPathInvalidHandle) {
    EXPECT_THROW({
        platform->getLibraryPath(INVALID_LIBRARY_HANDLE);
    }, std::out_of_range);
}

TEST_F(PlatformAbstractionTest, LoadedLibraryCount) {
    EXPECT_EQ(platform->getLoadedLibraryCount(), 0);

    std::string libPath1, libPath2;
#ifdef __linux__
    libPath1 = "libc.so.6";
    libPath2 = "libm.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath1 = "kernel32.dll";
    libPath2 = "user32.dll";
#elif defined(__APPLE__)
    libPath1 = "/usr/lib/libSystem.B.dylib";
    libPath2 = "/usr/lib/libc++.1.dylib";
#endif

    LibraryHandle handle1 = platform->loadLibrary(libPath1);
    EXPECT_EQ(platform->getLoadedLibraryCount(), 1);

    LibraryHandle handle2 = platform->loadLibrary(libPath2);
    EXPECT_EQ(platform->getLoadedLibraryCount(), 2);

    platform->unloadLibrary(handle1);
    EXPECT_EQ(platform->getLoadedLibraryCount(), 1);

    platform->unloadLibrary(handle2);
    EXPECT_EQ(platform->getLoadedLibraryCount(), 0);
}

TEST_F(PlatformAbstractionTest, IsLibraryLoaded) {
    std::string libPath;
#ifdef __linux__
    libPath = "libc.so.6";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    EXPECT_TRUE(platform->isLibraryLoaded(handle));

    platform->unloadLibrary(handle);
    EXPECT_FALSE(platform->isLibraryLoaded(handle));
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(PlatformAbstractionTest, GetLastErrorAfterFailure) {
    try {
        platform->loadLibrary("nonexistent_lib.so");
    } catch (const DynamicLoaderException&) {
        // Expected
    }

    std::string error = platform->getLastError();
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(PlatformAbstractionTest, ConcurrentLibraryLoading) {
    // Load different libraries concurrently to avoid handle conflicts
    std::vector<std::string> libPaths;
#ifdef __linux__
    libPaths = {"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1"};
#elif defined(_WIN32) || defined(_WIN64)
    libPaths = {"kernel32.dll", "user32.dll", "advapi32.dll", "shell32.dll", "ole32.dll"};
#elif defined(__APPLE__)
    libPaths = {
        "/usr/lib/libSystem.B.dylib",
        "/usr/lib/libc++.1.dylib",
        "/usr/lib/libobjc.A.dylib",
        "/usr/lib/libz.1.dylib",
        "/usr/lib/libiconv.2.dylib"
    };
#endif

    std::vector<std::thread> threads;
    std::vector<LibraryHandle> handles(libPaths.size(), INVALID_LIBRARY_HANDLE);

    // Load different libraries from multiple threads
    for (size_t i = 0; i < libPaths.size(); ++i) {
        threads.emplace_back([this, &libPaths, &handles, i]() {
            handles[i] = platform->loadLibrary(libPaths[i]);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all succeeded
    for (auto handle : handles) {
        EXPECT_NE(handle, INVALID_LIBRARY_HANDLE);
        EXPECT_TRUE(platform->isLibraryLoaded(handle));
    }

    // Clean up - unload in reverse order
    for (int i = handles.size() - 1; i >= 0; --i) {
        if (handles[i] != INVALID_LIBRARY_HANDLE) {
            platform->unloadLibrary(handles[i]);
        }
    }

    EXPECT_EQ(platform->getLoadedLibraryCount(), 0);
}

TEST_F(PlatformAbstractionTest, ConcurrentSymbolLookup) {
    std::string libPath, symbolName;
#ifdef __linux__
    libPath = "libc.so.6";
    symbolName = "printf";
#elif defined(_WIN32) || defined(_WIN64)
    libPath = "kernel32.dll";
    symbolName = "GetCurrentProcessId";
#elif defined(__APPLE__)
    libPath = "/usr/lib/libSystem.B.dylib";
    symbolName = "printf";
#endif

    LibraryHandle handle = platform->loadLibrary(libPath);
    ASSERT_NE(handle, INVALID_LIBRARY_HANDLE);

    std::vector<std::thread> threads;
    std::vector<void*> symbols(10, nullptr);

    // Look up same symbol from multiple threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, handle, &symbolName, &symbols, i]() {
            symbols[i] = platform->getSymbol(handle, symbolName);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all lookups succeeded and got the same address
    void* firstSymbol = symbols[0];
    EXPECT_NE(firstSymbol, nullptr);

    for (auto symbol : symbols) {
        EXPECT_EQ(symbol, firstSymbol);
    }

    platform->unloadLibrary(handle);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
