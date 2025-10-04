#ifndef CDMF_PLATFORM_TYPES_H
#define CDMF_PLATFORM_TYPES_H

#include <cstdint>

namespace cdmf {
namespace platform {

/**
 * @brief Enumeration of supported platforms
 *
 * This enum represents the different operating system platforms
 * that the CDMF framework can run on.
 */
enum class Platform {
    LINUX,      ///< Linux operating system
    WINDOWS,    ///< Windows operating system
    MACOS,      ///< macOS operating system
    UNKNOWN     ///< Unknown or unsupported platform
};

/**
 * @brief Opaque handle for dynamically loaded libraries
 *
 * This type represents a platform-specific handle to a dynamically
 * loaded library (shared object on Linux, DLL on Windows).
 *
 * Platform implementations:
 * - Linux: void* from dlopen()
 * - Windows: HMODULE from LoadLibrary()
 * - macOS: void* from dlopen()
 */
using LibraryHandle = void*;

/**
 * @brief Invalid/null library handle constant
 */
constexpr LibraryHandle INVALID_LIBRARY_HANDLE = nullptr;

/**
 * @brief Get string representation of platform
 *
 * @param platform The platform enum value
 * @return const char* Platform name as string
 */
inline const char* platformToString(Platform platform) {
    switch (platform) {
        case Platform::LINUX:   return "Linux";
        case Platform::WINDOWS: return "Windows";
        case Platform::MACOS:   return "macOS";
        case Platform::UNKNOWN: return "Unknown";
        default:                return "Invalid";
    }
}

/**
 * @brief Detect current platform at compile time
 *
 * @return Platform The detected platform
 */
inline Platform getCurrentPlatform() {
#if defined(__linux__)
    return Platform::LINUX;
#elif defined(_WIN32) || defined(_WIN64)
    return Platform::WINDOWS;
#elif defined(__APPLE__) && defined(__MACH__)
    return Platform::MACOS;
#else
    return Platform::UNKNOWN;
#endif
}

/**
 * @brief Get platform-specific library file extension
 *
 * @param platform The target platform
 * @return const char* File extension (e.g., ".so", ".dll", ".dylib")
 */
inline const char* getLibraryExtension(Platform platform) {
    switch (platform) {
        case Platform::LINUX:   return ".so";
        case Platform::WINDOWS: return ".dll";
        case Platform::MACOS:   return ".dylib";
        default:                return "";
    }
}

/**
 * @brief Get platform-specific library prefix
 *
 * @param platform The target platform
 * @return const char* Prefix (e.g., "lib" on Unix, "" on Windows)
 */
inline const char* getLibraryPrefix(Platform platform) {
    switch (platform) {
        case Platform::LINUX:
        case Platform::MACOS:   return "lib";
        case Platform::WINDOWS: return "";
        default:                return "";
    }
}

} // namespace platform
} // namespace cdmf

#endif // CDMF_PLATFORM_TYPES_H
