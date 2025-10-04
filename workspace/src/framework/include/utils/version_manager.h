#ifndef CDMF_VERSION_MANAGER_H
#define CDMF_VERSION_MANAGER_H

#include "utils/version.h"
#include "utils/version_range.h"
#include <vector>
#include <optional>

namespace cdmf {

/**
 * @brief Static utility class for version management operations
 *
 * Provides helper functions for:
 * - Version parsing and validation
 * - Version range operations
 * - Compatibility checking
 * - Best match selection
 */
class VersionManager {
public:
    // Delete constructors (static class)
    VersionManager() = delete;
    VersionManager(const VersionManager&) = delete;
    VersionManager& operator=(const VersionManager&) = delete;

    /**
     * @brief Parse a version string
     * @param versionString String to parse
     * @return Parsed Version object
     * @throws std::invalid_argument if format is invalid
     */
    static Version parse(const std::string& versionString) {
        return Version::parse(versionString);
    }

    /**
     * @brief Parse a version range string
     * @param rangeString Range to parse
     * @return Parsed VersionRange object
     * @throws std::invalid_argument if format is invalid
     */
    static VersionRange parseRange(const std::string& rangeString) {
        return VersionRange::parse(rangeString);
    }

    /**
     * @brief Check if two versions are compatible
     *
     * Versions are compatible if they have the same major version number.
     *
     * @param v1 First version
     * @param v2 Second version
     * @return true if compatible, false otherwise
     */
    static bool isCompatible(const Version& v1, const Version& v2) {
        return v1.isCompatibleWith(v2);
    }

    /**
     * @brief Compare two versions
     * @param v1 First version
     * @param v2 Second version
     * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
     */
    static int compare(const Version& v1, const Version& v2) {
        if (v1 < v2) return -1;
        if (v1 > v2) return 1;
        return 0;
    }

    /**
     * @brief Find the best matching version from available versions
     *
     * Selects the highest version that satisfies the range.
     *
     * @param available List of available versions
     * @param range Version range requirement
     * @return Best matching version, or std::nullopt if none match
     */
    static std::optional<Version> findBestMatch(
        const std::vector<Version>& available,
        const VersionRange& range);

    /**
     * @brief Find all versions that match a range
     *
     * @param available List of available versions
     * @param range Version range requirement
     * @return Vector of matching versions (unsorted)
     */
    static std::vector<Version> findAllMatches(
        const std::vector<Version>& available,
        const VersionRange& range);

    /**
     * @brief Get the latest version from a list
     *
     * @param versions List of versions
     * @return Latest version, or std::nullopt if list is empty
     */
    static std::optional<Version> getLatest(const std::vector<Version>& versions);

    /**
     * @brief Sort versions in ascending order
     *
     * @param versions List of versions to sort (modified in place)
     */
    static void sort(std::vector<Version>& versions);

    /**
     * @brief Sort versions in descending order
     *
     * @param versions List of versions to sort (modified in place)
     */
    static void sortDescending(std::vector<Version>& versions);

    /**
     * @brief Check if a version satisfies a range
     *
     * @param version Version to check
     * @param range Range to check against
     * @return true if version is in range, false otherwise
     */
    static bool satisfies(const Version& version, const VersionRange& range) {
        return range.includes(version);
    }

    /**
     * @brief Validate a version string without throwing
     *
     * @param versionString String to validate
     * @return true if valid, false otherwise
     */
    static bool isValidVersionString(const std::string& versionString);

    /**
     * @brief Validate a range string without throwing
     *
     * @param rangeString String to validate
     * @return true if valid, false otherwise
     */
    static bool isValidRangeString(const std::string& rangeString);
};

} // namespace cdmf

#endif // CDMF_VERSION_MANAGER_H
