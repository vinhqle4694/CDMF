#ifndef CDMF_VERSION_H
#define CDMF_VERSION_H

#include <string>
#include <ostream>
#include <stdexcept>

namespace cdmf {

/**
 * @brief Represents a semantic version (MAJOR.MINOR.PATCH[-QUALIFIER])
 *
 * This class implements semantic versioning following the format:
 * MAJOR.MINOR.PATCH[-QUALIFIER]
 *
 * Examples:
 * - 1.0.0
 * - 2.1.5
 * - 1.0.0-alpha
 * - 3.2.1-beta.1
 *
 * Versions are compared numerically by major, minor, and patch.
 * Qualifiers are compared lexicographically.
 */
class Version {
public:
    /**
     * @brief Default constructor creates version 0.0.0
     */
    Version();

    /**
     * @brief Construct a version with major, minor, patch
     * @param major Major version number
     * @param minor Minor version number
     * @param patch Patch version number
     * @param qualifier Optional qualifier string (e.g., "alpha", "beta")
     */
    Version(int major, int minor, int patch, const std::string& qualifier = "");

    /**
     * @brief Parse a version string
     * @param versionString String in format "MAJOR.MINOR.PATCH[-QUALIFIER]"
     * @return Parsed Version object
     * @throws std::invalid_argument if format is invalid
     */
    static Version parse(const std::string& versionString);

    /**
     * @brief Get major version number
     */
    int getMajor() const { return major_; }

    /**
     * @brief Get minor version number
     */
    int getMinor() const { return minor_; }

    /**
     * @brief Get patch version number
     */
    int getPatch() const { return patch_; }

    /**
     * @brief Get qualifier string
     */
    const std::string& getQualifier() const { return qualifier_; }

    /**
     * @brief Convert version to string representation
     * @return String in format "MAJOR.MINOR.PATCH[-QUALIFIER]"
     */
    std::string toString() const;

    /**
     * @brief Check if this version is compatible with another
     *
     * Versions are compatible if they have the same major version.
     * This follows semantic versioning rules where major version changes
     * indicate breaking changes.
     *
     * @param other Version to compare with
     * @return true if compatible, false otherwise
     */
    bool isCompatibleWith(const Version& other) const;

    /**
     * @brief Equality comparison
     */
    bool operator==(const Version& other) const;

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const Version& other) const;

    /**
     * @brief Less than comparison
     */
    bool operator<(const Version& other) const;

    /**
     * @brief Greater than comparison
     */
    bool operator>(const Version& other) const;

    /**
     * @brief Less than or equal comparison
     */
    bool operator<=(const Version& other) const;

    /**
     * @brief Greater than or equal comparison
     */
    bool operator>=(const Version& other) const;

    /**
     * @brief Stream output operator
     */
    friend std::ostream& operator<<(std::ostream& os, const Version& version);

private:
    int major_;
    int minor_;
    int patch_;
    std::string qualifier_;

    /**
     * @brief Compare two versions
     * @return -1 if this < other, 0 if equal, 1 if this > other
     */
    int compare(const Version& other) const;
};

} // namespace cdmf

#endif // CDMF_VERSION_H
