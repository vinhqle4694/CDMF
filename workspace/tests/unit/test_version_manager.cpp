/**
 * @file test_version_manager.cpp
 * @brief Comprehensive unit tests for VersionManager utility
 *
 * Tests include:
 * - Version parsing and validation
 * - Version range parsing
 * - Compatibility checking
 * - Best match selection
 * - Version comparison
 * - Sorting operations
 * - Edge cases and invalid inputs
 *
 * @author Version Manager Test Agent
 * @date 2025-10-05
 */

#include "utils/version_manager.h"
#include "utils/version.h"
#include "utils/version_range.h"
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>

using namespace cdmf;

// ============================================================================
// Test Fixtures
// ============================================================================

class VersionManagerTest : public ::testing::Test {
protected:
    std::vector<Version> createVersionList() {
        return {
            Version(1, 0, 0),
            Version(1, 2, 3),
            Version(1, 5, 0),
            Version(2, 0, 0),
            Version(2, 1, 0),
            Version(3, 0, 0)
        };
    }
};

// ============================================================================
// Version Parsing Tests
// ============================================================================

TEST_F(VersionManagerTest, ParseValidVersion) {
    auto version = VersionManager::parse("1.2.3");

    EXPECT_EQ(version.getMajor(), 1);
    EXPECT_EQ(version.getMinor(), 2);
    EXPECT_EQ(version.getPatch(), 3);
}

TEST_F(VersionManagerTest, ParseVersionWithQualifier) {
    auto version = VersionManager::parse("2.0.0-alpha");

    EXPECT_EQ(version.getMajor(), 2);
    EXPECT_EQ(version.getMinor(), 0);
    EXPECT_EQ(version.getPatch(), 0);
    EXPECT_EQ(version.getQualifier(), "alpha");
}

TEST_F(VersionManagerTest, ParseInvalidVersionThrows) {
    EXPECT_THROW(VersionManager::parse("invalid"), std::invalid_argument);
    EXPECT_THROW(VersionManager::parse("1.2"), std::invalid_argument);
    EXPECT_THROW(VersionManager::parse(""), std::invalid_argument);
    EXPECT_THROW(VersionManager::parse("a.b.c"), std::invalid_argument);
}

TEST_F(VersionManagerTest, IsValidVersionString) {
    EXPECT_TRUE(VersionManager::isValidVersionString("1.0.0"));
    EXPECT_TRUE(VersionManager::isValidVersionString("1.2.3"));
    EXPECT_TRUE(VersionManager::isValidVersionString("10.20.30"));
    EXPECT_TRUE(VersionManager::isValidVersionString("1.0.0-beta"));

    EXPECT_FALSE(VersionManager::isValidVersionString(""));
    EXPECT_FALSE(VersionManager::isValidVersionString("1.2"));
    EXPECT_FALSE(VersionManager::isValidVersionString("invalid"));
    EXPECT_FALSE(VersionManager::isValidVersionString("a.b.c"));
}

// ============================================================================
// Version Range Parsing Tests
// ============================================================================

TEST_F(VersionManagerTest, ParseValidRange) {
    auto range = VersionManager::parseRange("[1.0.0,2.0.0)");

    EXPECT_TRUE(range.includes(Version(1, 0, 0)));
    EXPECT_TRUE(range.includes(Version(1, 5, 0)));
    EXPECT_FALSE(range.includes(Version(2, 0, 0)));
}

TEST_F(VersionManagerTest, ParseRangeInvalidThrows) {
    EXPECT_THROW(VersionManager::parseRange("invalid"), std::invalid_argument);
    // Empty string is valid - creates unbounded range
    EXPECT_NO_THROW(VersionManager::parseRange(""));
}

TEST_F(VersionManagerTest, IsValidRangeString) {
    EXPECT_TRUE(VersionManager::isValidRangeString("[1.0.0,2.0.0)"));
    EXPECT_TRUE(VersionManager::isValidRangeString("[1.0.0,2.0.0]"));
    EXPECT_TRUE(VersionManager::isValidRangeString("(1.0.0,2.0.0)"));

    // Empty string is valid - creates unbounded range
    EXPECT_TRUE(VersionManager::isValidRangeString(""));
    // Simple version strings are valid - interpreted as [version,)
    EXPECT_TRUE(VersionManager::isValidRangeString("1.0.0"));

    EXPECT_FALSE(VersionManager::isValidRangeString("invalid"));
}

// ============================================================================
// Compatibility Tests
// ============================================================================

TEST_F(VersionManagerTest, IsCompatibleSameMajorVersion) {
    Version v1(1, 2, 3);
    Version v2(1, 5, 0);

    EXPECT_TRUE(VersionManager::isCompatible(v1, v2));
}

TEST_F(VersionManagerTest, IsCompatibleDifferentMajorVersion) {
    Version v1(1, 2, 3);
    Version v2(2, 0, 0);

    EXPECT_FALSE(VersionManager::isCompatible(v1, v2));
}

TEST_F(VersionManagerTest, IsCompatibleIdenticalVersions) {
    Version v1(1, 2, 3);
    Version v2(1, 2, 3);

    EXPECT_TRUE(VersionManager::isCompatible(v1, v2));
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST_F(VersionManagerTest, CompareLessThan) {
    Version v1(1, 0, 0);
    Version v2(2, 0, 0);

    EXPECT_EQ(VersionManager::compare(v1, v2), -1);
}

TEST_F(VersionManagerTest, CompareGreaterThan) {
    Version v1(2, 0, 0);
    Version v2(1, 0, 0);

    EXPECT_EQ(VersionManager::compare(v1, v2), 1);
}

TEST_F(VersionManagerTest, CompareEqual) {
    Version v1(1, 2, 3);
    Version v2(1, 2, 3);

    EXPECT_EQ(VersionManager::compare(v1, v2), 0);
}

TEST_F(VersionManagerTest, CompareMinorVersions) {
    Version v1(1, 2, 0);
    Version v2(1, 3, 0);

    EXPECT_EQ(VersionManager::compare(v1, v2), -1);
}

TEST_F(VersionManagerTest, ComparePatchVersions) {
    Version v1(1, 2, 3);
    Version v2(1, 2, 5);

    EXPECT_EQ(VersionManager::compare(v1, v2), -1);
}

// ============================================================================
// Best Match Tests
// ============================================================================

TEST_F(VersionManagerTest, FindBestMatchInRange) {
    auto versions = createVersionList();
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    auto best = VersionManager::findBestMatch(versions, range);

    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(best.value(), Version(1, 5, 0));  // Highest version in range
}

TEST_F(VersionManagerTest, FindBestMatchNoMatch) {
    auto versions = createVersionList();
    auto range = VersionRange::parse("[5.0.0,6.0.0)");

    auto best = VersionManager::findBestMatch(versions, range);

    EXPECT_FALSE(best.has_value());
}

TEST_F(VersionManagerTest, FindBestMatchEmptyList) {
    std::vector<Version> empty;
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    auto best = VersionManager::findBestMatch(empty, range);

    EXPECT_FALSE(best.has_value());
}

TEST_F(VersionManagerTest, FindBestMatchSingleVersion) {
    std::vector<Version> versions = {Version(1, 5, 0)};
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    auto best = VersionManager::findBestMatch(versions, range);

    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(best.value(), Version(1, 5, 0));
}

TEST_F(VersionManagerTest, FindBestMatchInclusiveUpperBound) {
    auto versions = createVersionList();
    auto range = VersionRange::parse("[1.0.0,2.0.0]");

    auto best = VersionManager::findBestMatch(versions, range);

    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(best.value(), Version(2, 0, 0));
}

// ============================================================================
// All Matches Tests
// ============================================================================

TEST_F(VersionManagerTest, FindAllMatches) {
    auto versions = createVersionList();
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    auto matches = VersionManager::findAllMatches(versions, range);

    EXPECT_EQ(matches.size(), 3);  // 1.0.0, 1.2.3, 1.5.0

    EXPECT_NE(std::find(matches.begin(), matches.end(), Version(1, 0, 0)), matches.end());
    EXPECT_NE(std::find(matches.begin(), matches.end(), Version(1, 2, 3)), matches.end());
    EXPECT_NE(std::find(matches.begin(), matches.end(), Version(1, 5, 0)), matches.end());
}

TEST_F(VersionManagerTest, FindAllMatchesNone) {
    auto versions = createVersionList();
    auto range = VersionRange::parse("[10.0.0,20.0.0)");

    auto matches = VersionManager::findAllMatches(versions, range);

    EXPECT_TRUE(matches.empty());
}

TEST_F(VersionManagerTest, FindAllMatchesAll) {
    auto versions = createVersionList();
    auto range = VersionRange::parse("[0.0.0,10.0.0)");

    auto matches = VersionManager::findAllMatches(versions, range);

    EXPECT_EQ(matches.size(), versions.size());
}

// ============================================================================
// Latest Version Tests
// ============================================================================

TEST_F(VersionManagerTest, GetLatestVersion) {
    auto versions = createVersionList();

    auto latest = VersionManager::getLatest(versions);

    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest.value(), Version(3, 0, 0));
}

TEST_F(VersionManagerTest, GetLatestEmptyList) {
    std::vector<Version> empty;

    auto latest = VersionManager::getLatest(empty);

    EXPECT_FALSE(latest.has_value());
}

TEST_F(VersionManagerTest, GetLatestSingleVersion) {
    std::vector<Version> single = {Version(1, 2, 3)};

    auto latest = VersionManager::getLatest(single);

    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest.value(), Version(1, 2, 3));
}

TEST_F(VersionManagerTest, GetLatestUnorderedList) {
    std::vector<Version> unordered = {
        Version(2, 0, 0),
        Version(1, 0, 0),
        Version(3, 0, 0),
        Version(1, 5, 0)
    };

    auto latest = VersionManager::getLatest(unordered);

    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest.value(), Version(3, 0, 0));
}

// ============================================================================
// Sorting Tests
// ============================================================================

TEST_F(VersionManagerTest, SortAscending) {
    std::vector<Version> versions = {
        Version(3, 0, 0),
        Version(1, 0, 0),
        Version(2, 1, 0),
        Version(1, 5, 0)
    };

    VersionManager::sort(versions);

    EXPECT_EQ(versions[0], Version(1, 0, 0));
    EXPECT_EQ(versions[1], Version(1, 5, 0));
    EXPECT_EQ(versions[2], Version(2, 1, 0));
    EXPECT_EQ(versions[3], Version(3, 0, 0));
}

TEST_F(VersionManagerTest, SortDescending) {
    std::vector<Version> versions = {
        Version(1, 0, 0),
        Version(3, 0, 0),
        Version(1, 5, 0),
        Version(2, 1, 0)
    };

    VersionManager::sortDescending(versions);

    EXPECT_EQ(versions[0], Version(3, 0, 0));
    EXPECT_EQ(versions[1], Version(2, 1, 0));
    EXPECT_EQ(versions[2], Version(1, 5, 0));
    EXPECT_EQ(versions[3], Version(1, 0, 0));
}

TEST_F(VersionManagerTest, SortEmptyList) {
    std::vector<Version> empty;

    // Should not crash
    VersionManager::sort(empty);
    VersionManager::sortDescending(empty);

    EXPECT_TRUE(empty.empty());
}

TEST_F(VersionManagerTest, SortSingleElement) {
    std::vector<Version> single = {Version(1, 2, 3)};

    VersionManager::sort(single);

    EXPECT_EQ(single.size(), 1);
    EXPECT_EQ(single[0], Version(1, 2, 3));
}

TEST_F(VersionManagerTest, SortAlreadySorted) {
    std::vector<Version> sorted = {
        Version(1, 0, 0),
        Version(1, 2, 3),
        Version(2, 0, 0)
    };

    VersionManager::sort(sorted);

    EXPECT_EQ(sorted[0], Version(1, 0, 0));
    EXPECT_EQ(sorted[1], Version(1, 2, 3));
    EXPECT_EQ(sorted[2], Version(2, 0, 0));
}

// ============================================================================
// Satisfies Range Tests
// ============================================================================

TEST_F(VersionManagerTest, SatisfiesRange) {
    Version version(1, 5, 0);
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    EXPECT_TRUE(VersionManager::satisfies(version, range));
}

TEST_F(VersionManagerTest, DoesNotSatisfyRange) {
    Version version(2, 5, 0);
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    EXPECT_FALSE(VersionManager::satisfies(version, range));
}

TEST_F(VersionManagerTest, SatisfiesRangeBoundaryInclusive) {
    Version version(1, 0, 0);
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    EXPECT_TRUE(VersionManager::satisfies(version, range));
}

TEST_F(VersionManagerTest, DoesNotSatisfyRangeBoundaryExclusive) {
    Version version(2, 0, 0);
    auto range = VersionRange::parse("[1.0.0,2.0.0)");

    EXPECT_FALSE(VersionManager::satisfies(version, range));
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST_F(VersionManagerTest, LargeVersionNumbers) {
    auto version = VersionManager::parse("999.888.777");

    EXPECT_EQ(version.getMajor(), 999);
    EXPECT_EQ(version.getMinor(), 888);
    EXPECT_EQ(version.getPatch(), 777);
}

TEST_F(VersionManagerTest, VersionWithLongQualifier) {
    auto version = VersionManager::parse("1.0.0-this-is-a-very-long-qualifier-string");

    EXPECT_EQ(version.getQualifier(), "this-is-a-very-long-qualifier-string");
}

TEST_F(VersionManagerTest, ManyVersionsInList) {
    std::vector<Version> many;
    for (int i = 0; i < 1000; ++i) {
        many.push_back(Version(i / 100, (i / 10) % 10, i % 10));
    }

    auto range = VersionRange::parse("[5.0.0,6.0.0)");
    auto matches = VersionManager::findAllMatches(many, range);

    EXPECT_GT(matches.size(), 0);

    auto best = VersionManager::findBestMatch(many, range);
    EXPECT_TRUE(best.has_value());
}

TEST_F(VersionManagerTest, SortManyVersions) {
    std::vector<Version> many;
    for (int i = 999; i >= 0; --i) {
        many.push_back(Version(i / 100, (i / 10) % 10, i % 10));
    }

    VersionManager::sort(many);

    // Verify ascending order
    for (size_t i = 1; i < many.size(); ++i) {
        EXPECT_TRUE(many[i - 1] < many[i] || many[i - 1] == many[i]);
    }
}

TEST_F(VersionManagerTest, CompareWithQualifiers) {
    Version v1(1, 0, 0, "alpha");
    Version v2(1, 0, 0, "beta");
    Version v3(1, 0, 0);

    // Versions with qualifiers are typically less than without
    int result = VersionManager::compare(v1, v3);
    EXPECT_NE(result, 0);
}

TEST_F(VersionManagerTest, FindBestMatchWithQualifiers) {
    std::vector<Version> versions = {
        Version(1, 0, 0, "alpha"),
        Version(1, 0, 0, "beta"),
        Version(1, 0, 0),
        Version(1, 1, 0)
    };

    auto range = VersionRange::parse("[1.0.0,2.0.0)");
    auto best = VersionManager::findBestMatch(versions, range);

    ASSERT_TRUE(best.has_value());
    // Should select highest version
    EXPECT_EQ(best.value(), Version(1, 1, 0));
}

TEST_F(VersionManagerTest, DuplicateVersionsInList) {
    std::vector<Version> versions = {
        Version(1, 0, 0),
        Version(1, 0, 0),
        Version(2, 0, 0),
        Version(2, 0, 0)
    };

    auto range = VersionRange::parse("[1.0.0,3.0.0)");
    auto matches = VersionManager::findAllMatches(versions, range);

    // Should include duplicates
    EXPECT_EQ(matches.size(), 4);
}

TEST_F(VersionManagerTest, EmptyQualifier) {
    // Version format with trailing dash is invalid
    EXPECT_THROW(VersionManager::parse("1.0.0-"), std::invalid_argument);
}

TEST_F(VersionManagerTest, ZeroVersionComponents) {
    auto version = VersionManager::parse("0.0.0");

    EXPECT_EQ(version.getMajor(), 0);
    EXPECT_EQ(version.getMinor(), 0);
    EXPECT_EQ(version.getPatch(), 0);

    auto versions = std::vector<Version>{version};
    auto latest = VersionManager::getLatest(versions);

    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest.value(), Version(0, 0, 0));
}

TEST_F(VersionManagerTest, RangeWithSameVersionBounds) {
    auto range = VersionRange::parse("[1.0.0,1.0.0]");

    EXPECT_TRUE(VersionManager::satisfies(Version(1, 0, 0), range));
    EXPECT_FALSE(VersionManager::satisfies(Version(1, 0, 1), range));
}

TEST_F(VersionManagerTest, CompatibilityWithMajorZero) {
    Version v1(0, 1, 0);
    Version v2(0, 2, 0);

    // Major version 0 might have different compatibility rules
    EXPECT_TRUE(VersionManager::isCompatible(v1, v2));
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
