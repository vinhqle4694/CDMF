#include <gtest/gtest.h>
#include "utils/version.h"
#include "utils/version_range.h"
#include "utils/version_manager.h"
#include <sstream>

using namespace cdmf;

// ============================================================================
// Version Tests
// ============================================================================

TEST(VersionTest, DefaultConstructor) {
    Version v;
    EXPECT_EQ(0, v.getMajor());
    EXPECT_EQ(0, v.getMinor());
    EXPECT_EQ(0, v.getPatch());
    EXPECT_EQ("", v.getQualifier());
}

TEST(VersionTest, ParameterConstructor) {
    Version v(1, 2, 3, "alpha");
    EXPECT_EQ(1, v.getMajor());
    EXPECT_EQ(2, v.getMinor());
    EXPECT_EQ(3, v.getPatch());
    EXPECT_EQ("alpha", v.getQualifier());
}

TEST(VersionTest, ConstructorNoQualifier) {
    Version v(2, 0, 1);
    EXPECT_EQ(2, v.getMajor());
    EXPECT_EQ(0, v.getMinor());
    EXPECT_EQ(1, v.getPatch());
    EXPECT_EQ("", v.getQualifier());
}

TEST(VersionTest, ConstructorInvalidNegative) {
    EXPECT_THROW(Version(-1, 0, 0), std::invalid_argument);
    EXPECT_THROW(Version(0, -1, 0), std::invalid_argument);
    EXPECT_THROW(Version(0, 0, -1), std::invalid_argument);
}

TEST(VersionTest, ParseValid) {
    Version v1 = Version::parse("1.2.3");
    EXPECT_EQ(1, v1.getMajor());
    EXPECT_EQ(2, v1.getMinor());
    EXPECT_EQ(3, v1.getPatch());
    EXPECT_EQ("", v1.getQualifier());

    Version v2 = Version::parse("2.0.1-beta");
    EXPECT_EQ(2, v2.getMajor());
    EXPECT_EQ(0, v2.getMinor());
    EXPECT_EQ(1, v2.getPatch());
    EXPECT_EQ("beta", v2.getQualifier());

    Version v3 = Version::parse("1.0.0-alpha.1");
    EXPECT_EQ("alpha.1", v3.getQualifier());
}

TEST(VersionTest, ParseInvalid) {
    EXPECT_THROW(Version::parse(""), std::invalid_argument);
    EXPECT_THROW(Version::parse("1.2"), std::invalid_argument);
    EXPECT_THROW(Version::parse("1.2.3.4"), std::invalid_argument);
    EXPECT_THROW(Version::parse("a.b.c"), std::invalid_argument);
    EXPECT_THROW(Version::parse("1.2.3-"), std::invalid_argument);
}

TEST(VersionTest, ToStringWithoutQualifier) {
    Version v(1, 2, 3);
    EXPECT_EQ("1.2.3", v.toString());
}

TEST(VersionTest, ToStringWithQualifier) {
    Version v(1, 2, 3, "alpha");
    EXPECT_EQ("1.2.3-alpha", v.toString());
}

TEST(VersionTest, Compatibility) {
    Version v1(1, 0, 0);
    Version v2(1, 5, 2);
    Version v3(2, 0, 0);

    EXPECT_TRUE(v1.isCompatibleWith(v2));
    EXPECT_TRUE(v2.isCompatibleWith(v1));
    EXPECT_FALSE(v1.isCompatibleWith(v3));
    EXPECT_FALSE(v3.isCompatibleWith(v1));
}

TEST(VersionTest, EqualityOperator) {
    Version v1(1, 2, 3);
    Version v2(1, 2, 3);
    Version v3(1, 2, 4);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_FALSE(v1 != v2);
    EXPECT_TRUE(v1 != v3);
}

TEST(VersionTest, ComparisonOperators) {
    Version v1(1, 0, 0);
    Version v2(1, 0, 1);
    Version v3(1, 1, 0);
    Version v4(2, 0, 0);

    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v2 < v3);
    EXPECT_TRUE(v3 < v4);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 <= v1);

    EXPECT_TRUE(v4 > v3);
    EXPECT_TRUE(v3 > v2);
    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v4 >= v3);
    EXPECT_TRUE(v4 >= v4);
}

TEST(VersionTest, QualifierComparison) {
    Version v1(1, 0, 0, "alpha");
    Version v2(1, 0, 0, "beta");
    Version v3(1, 0, 0); // No qualifier (release)

    EXPECT_TRUE(v1 < v2);    // alpha < beta
    EXPECT_TRUE(v1 < v3);    // pre-release < release
    EXPECT_TRUE(v2 < v3);    // pre-release < release
}

TEST(VersionTest, StreamOutput) {
    Version v(1, 2, 3, "beta");
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ("1.2.3-beta", oss.str());
}

// ============================================================================
// VersionRange Tests
// ============================================================================

TEST(VersionRangeTest, DefaultConstructor) {
    VersionRange range;
    Version v(1, 0, 0);
    EXPECT_TRUE(range.includes(v));  // Unbounded includes everything
}

TEST(VersionRangeTest, ParseInclusiveRange) {
    VersionRange range = VersionRange::parse("[1.0.0,2.0.0]");
    EXPECT_TRUE(range.includes(Version(1, 0, 0)));
    EXPECT_TRUE(range.includes(Version(1, 5, 0)));
    EXPECT_TRUE(range.includes(Version(2, 0, 0)));
    EXPECT_FALSE(range.includes(Version(0, 9, 9)));
    EXPECT_FALSE(range.includes(Version(2, 0, 1)));
}

TEST(VersionRangeTest, ParseExclusiveRange) {
    VersionRange range = VersionRange::parse("(1.0.0,2.0.0)");
    EXPECT_FALSE(range.includes(Version(1, 0, 0)));
    EXPECT_TRUE(range.includes(Version(1, 5, 0)));
    EXPECT_FALSE(range.includes(Version(2, 0, 0)));
}

TEST(VersionRangeTest, ParseMixedRange) {
    VersionRange range1 = VersionRange::parse("[1.0.0,2.0.0)");
    EXPECT_TRUE(range1.includes(Version(1, 0, 0)));
    EXPECT_FALSE(range1.includes(Version(2, 0, 0)));

    VersionRange range2 = VersionRange::parse("(1.0.0,2.0.0]");
    EXPECT_FALSE(range2.includes(Version(1, 0, 0)));
    EXPECT_TRUE(range2.includes(Version(2, 0, 0)));
}

TEST(VersionRangeTest, ParseUnboundedAbove) {
    VersionRange range = VersionRange::parse("[1.0.0,)");
    EXPECT_TRUE(range.includes(Version(1, 0, 0)));
    EXPECT_TRUE(range.includes(Version(10, 0, 0)));
    EXPECT_FALSE(range.includes(Version(0, 9, 9)));
}

TEST(VersionRangeTest, ParseUnboundedBelow) {
    VersionRange range = VersionRange::parse("(,2.0.0)");
    EXPECT_TRUE(range.includes(Version(0, 0, 1)));
    EXPECT_TRUE(range.includes(Version(1, 9, 9)));
    EXPECT_FALSE(range.includes(Version(2, 0, 0)));
}

TEST(VersionRangeTest, ParseSimpleVersion) {
    // "1.0.0" is interpreted as [1.0.0,)
    VersionRange range = VersionRange::parse("1.0.0");
    EXPECT_TRUE(range.includes(Version(1, 0, 0)));
    EXPECT_TRUE(range.includes(Version(2, 0, 0)));
    EXPECT_FALSE(range.includes(Version(0, 9, 9)));
}

TEST(VersionRangeTest, ParseInvalid) {
    EXPECT_THROW(VersionRange::parse("[1.0.0,0.5.0]"), std::invalid_argument);
    EXPECT_THROW(VersionRange::parse("(1.0.0,1.0.0)"), std::invalid_argument);
    EXPECT_THROW(VersionRange::parse("[a.b.c,2.0.0]"), std::invalid_argument);
}

TEST(VersionRangeTest, ToString) {
    VersionRange range1 = VersionRange::parse("[1.0.0,2.0.0]");
    EXPECT_EQ("[1.0.0,2.0.0]", range1.toString());

    VersionRange range2 = VersionRange::parse("[1.0.0,)");
    EXPECT_EQ("[1.0.0,)", range2.toString());
}

TEST(VersionRangeTest, EqualityOperator) {
    VersionRange range1 = VersionRange::parse("[1.0.0,2.0.0]");
    VersionRange range2 = VersionRange::parse("[1.0.0,2.0.0]");
    VersionRange range3 = VersionRange::parse("[1.0.0,2.0.0)");

    EXPECT_TRUE(range1 == range2);
    EXPECT_FALSE(range1 == range3);
    EXPECT_TRUE(range1 != range3);
}

// ============================================================================
// VersionManager Tests
// ============================================================================

TEST(VersionManagerTest, Parse) {
    Version v = VersionManager::parse("1.2.3");
    EXPECT_EQ(1, v.getMajor());
    EXPECT_EQ(2, v.getMinor());
    EXPECT_EQ(3, v.getPatch());
}

TEST(VersionManagerTest, ParseRange) {
    VersionRange range = VersionManager::parseRange("[1.0.0,2.0.0]");
    EXPECT_TRUE(range.includes(Version(1, 5, 0)));
}

TEST(VersionManagerTest, IsCompatible) {
    Version v1(1, 0, 0);
    Version v2(1, 5, 0);
    Version v3(2, 0, 0);

    EXPECT_TRUE(VersionManager::isCompatible(v1, v2));
    EXPECT_FALSE(VersionManager::isCompatible(v1, v3));
}

TEST(VersionManagerTest, Compare) {
    Version v1(1, 0, 0);
    Version v2(1, 5, 0);
    Version v3(1, 0, 0);

    EXPECT_EQ(-1, VersionManager::compare(v1, v2));
    EXPECT_EQ(1, VersionManager::compare(v2, v1));
    EXPECT_EQ(0, VersionManager::compare(v1, v3));
}

TEST(VersionManagerTest, FindBestMatch) {
    std::vector<Version> available = {
        Version(1, 0, 0),
        Version(1, 5, 0),
        Version(2, 0, 0),
        Version(2, 1, 0)
    };

    VersionRange range = VersionRange::parse("[1.0.0,2.0.0]");
    auto best = VersionManager::findBestMatch(available, range);

    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(Version(2, 0, 0), best.value());
}

TEST(VersionManagerTest, FindBestMatchNoMatch) {
    std::vector<Version> available = {
        Version(1, 0, 0),
        Version(1, 5, 0)
    };

    VersionRange range = VersionRange::parse("[2.0.0,3.0.0]");
    auto best = VersionManager::findBestMatch(available, range);

    EXPECT_FALSE(best.has_value());
}

TEST(VersionManagerTest, FindAllMatches) {
    std::vector<Version> available = {
        Version(1, 0, 0),
        Version(1, 5, 0),
        Version(2, 0, 0),
        Version(2, 1, 0)
    };

    VersionRange range = VersionRange::parse("[1.0.0,2.0.0)");
    auto matches = VersionManager::findAllMatches(available, range);

    EXPECT_EQ(2u, matches.size());
}

TEST(VersionManagerTest, GetLatest) {
    std::vector<Version> versions = {
        Version(1, 0, 0),
        Version(2, 1, 0),
        Version(1, 5, 0)
    };

    auto latest = VersionManager::getLatest(versions);
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(Version(2, 1, 0), latest.value());
}

TEST(VersionManagerTest, GetLatestEmpty) {
    std::vector<Version> versions;
    auto latest = VersionManager::getLatest(versions);
    EXPECT_FALSE(latest.has_value());
}

TEST(VersionManagerTest, Sort) {
    std::vector<Version> versions = {
        Version(2, 0, 0),
        Version(1, 0, 0),
        Version(1, 5, 0)
    };

    VersionManager::sort(versions);

    EXPECT_EQ(Version(1, 0, 0), versions[0]);
    EXPECT_EQ(Version(1, 5, 0), versions[1]);
    EXPECT_EQ(Version(2, 0, 0), versions[2]);
}

TEST(VersionManagerTest, SortDescending) {
    std::vector<Version> versions = {
        Version(1, 0, 0),
        Version(2, 0, 0),
        Version(1, 5, 0)
    };

    VersionManager::sortDescending(versions);

    EXPECT_EQ(Version(2, 0, 0), versions[0]);
    EXPECT_EQ(Version(1, 5, 0), versions[1]);
    EXPECT_EQ(Version(1, 0, 0), versions[2]);
}

TEST(VersionManagerTest, Satisfies) {
    VersionRange range = VersionRange::parse("[1.0.0,2.0.0]");
    EXPECT_TRUE(VersionManager::satisfies(Version(1, 5, 0), range));
    EXPECT_FALSE(VersionManager::satisfies(Version(2, 5, 0), range));
}

TEST(VersionManagerTest, IsValidVersionString) {
    EXPECT_TRUE(VersionManager::isValidVersionString("1.2.3"));
    EXPECT_TRUE(VersionManager::isValidVersionString("1.0.0-alpha"));
    EXPECT_FALSE(VersionManager::isValidVersionString("1.2"));
    EXPECT_FALSE(VersionManager::isValidVersionString("invalid"));
}

TEST(VersionManagerTest, IsValidRangeString) {
    EXPECT_TRUE(VersionManager::isValidRangeString("[1.0.0,2.0.0]"));
    EXPECT_TRUE(VersionManager::isValidRangeString("[1.0.0,)"));
    EXPECT_TRUE(VersionManager::isValidRangeString("1.0.0"));
    EXPECT_FALSE(VersionManager::isValidRangeString("[invalid,2.0.0]"));
}
