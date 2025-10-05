#ifndef CDMF_VERSION_RANGE_H
#define CDMF_VERSION_RANGE_H

#include "utils/version.h"
#include <string>

namespace cdmf {

/**
 * @brief Represents a version range for dependency matching
 *
 * Supports standard interval notation:
 * - [1.0.0,2.0.0] - Inclusive range from 1.0.0 to 2.0.0
 * - [1.0.0,2.0.0) - Inclusive start, exclusive end
 * - (1.0.0,2.0.0] - Exclusive start, inclusive end
 * - (1.0.0,2.0.0) - Exclusive range
 * - [1.0.0,) - Version 1.0.0 or higher
 * - (,2.0.0) - Any version below 2.0.0
 *
 * Special cases:
 * - Empty range matches all versions
 * - Single version "1.0.0" is treated as [1.0.0,)
 */
class VersionRange {
public:
    /**
     * @brief Default constructor creates an unbounded range (matches all)
     */
    VersionRange();

    /**
     * @brief Construct a version range from a single version (creates [version,) range)
     * @param version Minimum version (unbounded above)
     */
    explicit VersionRange(const Version& version);

    /**
     * @brief Construct a version range
     * @param minimum Minimum version
     * @param maximum Maximum version
     * @param includeMinimum Include minimum version in range
     * @param includeMaximum Include maximum version in range
     */
    VersionRange(const Version& minimum, const Version& maximum,
                 bool includeMinimum, bool includeMaximum);

    /**
     * @brief Parse a version range string
     * @param rangeString Range in interval notation
     * @return Parsed VersionRange object
     * @throws std::invalid_argument if format is invalid
     *
     * Examples:
     * - "[1.0.0,2.0.0]" - From 1.0.0 to 2.0.0 inclusive
     * - "[1.0.0,)" - 1.0.0 or higher
     * - "1.0.0" - Interpreted as [1.0.0,)
     */
    static VersionRange parse(const std::string& rangeString);

    /**
     * @brief Check if a version is included in this range
     * @param version Version to check
     * @return true if version is in range, false otherwise
     */
    bool includes(const Version& version) const;

    /**
     * @brief Get minimum version
     */
    const Version& getMinimum() const { return minimum_; }

    /**
     * @brief Get maximum version
     */
    const Version& getMaximum() const { return maximum_; }

    /**
     * @brief Check if minimum is included
     */
    bool isMinimumInclusive() const { return includeMinimum_; }

    /**
     * @brief Check if maximum is included
     */
    bool isMaximumInclusive() const { return includeMaximum_; }

    /**
     * @brief Check if range has no upper bound
     */
    bool isUnboundedAbove() const { return !hasMaximum_; }

    /**
     * @brief Check if range has no lower bound
     */
    bool isUnboundedBelow() const { return !hasMinimum_; }

    /**
     * @brief Convert range to string representation
     */
    std::string toString() const;

    /**
     * @brief Equality comparison
     */
    bool operator==(const VersionRange& other) const;

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const VersionRange& other) const;

private:
    Version minimum_;
    Version maximum_;
    bool includeMinimum_;
    bool includeMaximum_;
    bool hasMinimum_;
    bool hasMaximum_;
};

} // namespace cdmf

#endif // CDMF_VERSION_RANGE_H
