#include <gtest/gtest.h>
#include "module/module_impl.h"
#include "module/manifest_parser.h"

using namespace cdmf;

// ============================================================================
// Module Implementation Tests
// ============================================================================

// Note: These tests are limited because ModuleImpl requires actual shared libraries
// and Framework integration. Full testing will be done in integration tests.

TEST(ModuleImplTest, Construction) {
    // Create a minimal manifest
    std::string jsonStr = R"({
        "module": {
            "symbolic-name": "com.example.test",
            "version": "1.0.0"
        }
    })";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    // Can't create ModuleImpl without valid handle and framework
    // This test just verifies the API compiles
    EXPECT_EQ("com.example.test", manifest.symbolicName);
    EXPECT_EQ(Version(1, 0, 0), manifest.version);
}

TEST(ModuleImplTest, GettersFromManifest) {
    std::string jsonStr = R"({
        "module": {
            "symbolic-name": "com.example.test",
            "version": "2.1.3",
            "name": "Test Module",
            "description": "A test module",
            "vendor": "Test Corp",
            "category": "testing"
        }
    })";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    EXPECT_EQ("com.example.test", manifest.symbolicName);
    EXPECT_EQ(Version(2, 1, 3), manifest.version);
    EXPECT_EQ("Test Module", manifest.name);
    EXPECT_EQ("A test module", manifest.description);
    EXPECT_EQ("Test Corp", manifest.vendor);
    EXPECT_EQ("testing", manifest.category);
}

TEST(ModuleExceptionTest, ExceptionWithState) {
    ModuleException ex("Test error", ModuleState::INSTALLED);
    std::string msg = ex.what();

    EXPECT_NE(msg.find("Test error"), std::string::npos);
    EXPECT_NE(msg.find("INSTALLED"), std::string::npos);
}

// Full integration tests will be added later with:
// - Actual module shared libraries
// - Framework instance
// - Service registry
// - Event dispatcher integration
