#include <gtest/gtest.h>
#include "module/manifest_parser.h"
#include <nlohmann/json.hpp>

using namespace cdmf;

// ============================================================================
// Manifest Parser Tests
// ============================================================================

TEST(ManifestParserTest, ParseMinimalManifest) {
    std::string jsonStr = R"JSON({
        "module": {
            "symbolic-name": "com.example.test",
            "version": "1.0.0"
        }
    })JSON";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    EXPECT_EQ("com.example.test", manifest.symbolicName);
    EXPECT_EQ(Version(1, 0, 0), manifest.version);
    EXPECT_EQ("com.example.test", manifest.name); // defaults to symbolic name
    EXPECT_FALSE(manifest.autoStart);
}

TEST(ManifestParserTest, ParseFullManifest) {
    std::string jsonStr = R"JSON({
        "module": {
            "symbolic-name": "com.example.full",
            "version": "2.1.3",
            "name": "Full Test Module",
            "description": "A complete module manifest",
            "vendor": "Example Corp",
            "category": "utility",
            "activator": "FullModuleActivator",
            "auto-start": true
        },
        "dependencies": [
            {
                "symbolic-name": "com.example.logger",
                "version-range": "[1.0.0,2.0.0)",
                "optional": false
            }
        ],
        "exported-packages": [
            {
                "package": "com.example.full.api",
                "version": "2.1.3"
            }
        ],
        "imported-packages": [
            {
                "package": "com.example.logger.api",
                "version-range": "[1.0.0,2.0.0)"
            }
        ],
        "services": {
            "provides": [
                {
                    "interface": "com.example.IProcessor",
                    "properties": {
                        "service.ranking": 100
                    }
                }
            ],
            "requires": [
                {
                    "interface": "com.example.ILogger",
                    "cardinality": "1..1"
                }
            ]
        },
        "security": {
            "permissions": [
                "file:read:/data/**",
                "network:connect:localhost:8080"
            ],
            "sandbox": {
                "enabled": false
            }
        }
    })JSON";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    // Basic fields
    EXPECT_EQ("com.example.full", manifest.symbolicName);
    EXPECT_EQ(Version(2, 1, 3), manifest.version);
    EXPECT_EQ("Full Test Module", manifest.name);
    EXPECT_EQ("A complete module manifest", manifest.description);
    EXPECT_EQ("Example Corp", manifest.vendor);
    EXPECT_EQ("utility", manifest.category);
    EXPECT_EQ("FullModuleActivator", manifest.activator);
    EXPECT_TRUE(manifest.autoStart);

    // Dependencies
    ASSERT_EQ(1u, manifest.dependencies.size());
    EXPECT_EQ("com.example.logger", manifest.dependencies[0].symbolicName);
    EXPECT_FALSE(manifest.dependencies[0].optional);

    // Exported packages
    ASSERT_EQ(1u, manifest.exportedPackages.size());
    EXPECT_EQ("com.example.full.api", manifest.exportedPackages[0].package);
    EXPECT_EQ(Version(2, 1, 3), manifest.exportedPackages[0].version);

    // Imported packages
    ASSERT_EQ(1u, manifest.importedPackages.size());
    EXPECT_EQ("com.example.logger.api", manifest.importedPackages[0].package);

    // Services
    ASSERT_EQ(1u, manifest.providedServices.size());
    EXPECT_EQ("com.example.IProcessor", manifest.providedServices[0].interface);

    ASSERT_EQ(1u, manifest.requiredServices.size());
    EXPECT_EQ("com.example.ILogger", manifest.requiredServices[0].interface);
    EXPECT_EQ("1..1", manifest.requiredServices[0].cardinality);

    // Security
    ASSERT_EQ(2u, manifest.permissions.size());
    EXPECT_EQ("file:read:/data/**", manifest.permissions[0]);
    EXPECT_FALSE(manifest.sandboxEnabled);
}

TEST(ManifestParserTest, MissingModuleSection) {
    std::string jsonStr = R"JSON({
        "dependencies": []
    })JSON";

    EXPECT_THROW(ManifestParser::parseString(jsonStr), std::runtime_error);
}

TEST(ManifestParserTest, MissingSymbolicName) {
    std::string jsonStr = R"JSON({
        "module": {
            "version": "1.0.0"
        }
    })JSON";

    EXPECT_THROW(ManifestParser::parseString(jsonStr), std::runtime_error);
}

TEST(ManifestParserTest, MissingVersion) {
    std::string jsonStr = R"JSON({
        "module": {
            "symbolic-name": "com.example.test"
        }
    })JSON";

    EXPECT_THROW(ManifestParser::parseString(jsonStr), std::runtime_error);
}

TEST(ManifestParserTest, InvalidJSON) {
    std::string jsonStr = "{ invalid json }";

    EXPECT_THROW(ManifestParser::parseString(jsonStr), std::runtime_error);
}

TEST(ManifestParserTest, ParseDependenciesOptional) {
    std::string jsonStr = R"JSON({
        "module": {
            "symbolic-name": "com.example.test",
            "version": "1.0.0"
        },
        "dependencies": [
            {
                "symbolic-name": "com.example.optional",
                "version-range": "[1.0.0,2.0.0)",
                "optional": true
            }
        ]
    })JSON";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    ASSERT_EQ(1u, manifest.dependencies.size());
    EXPECT_TRUE(manifest.dependencies[0].optional);
}

TEST(ManifestParserTest, DefaultVersionRange) {
    std::string jsonStr = R"JSON({
        "module": {
            "symbolic-name": "com.example.test",
            "version": "1.0.0"
        },
        "dependencies": [
            {
                "symbolic-name": "com.example.dep"
            }
        ]
    })JSON";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    ASSERT_EQ(1u, manifest.dependencies.size());
    // Should have default version range [0.0.0,)
    EXPECT_TRUE(manifest.dependencies[0].versionRange.includes(Version(0, 0, 0)));
    EXPECT_TRUE(manifest.dependencies[0].versionRange.includes(Version(999, 999, 999)));
}

TEST(ManifestParserTest, EmptyDependencies) {
    std::string jsonStr = R"JSON({
        "module": {
            "symbolic-name": "com.example.test",
            "version": "1.0.0"
        },
        "dependencies": []
    })JSON";

    ModuleManifest manifest = ManifestParser::parseString(jsonStr);

    EXPECT_EQ(0u, manifest.dependencies.size());
}

TEST(ManifestParserTest, ParseFromJSON) {
    nlohmann::json json = {
        {"module", {
            {"symbolic-name", "com.example.test"},
            {"version", "1.0.0"}
        }}
    };

    ModuleManifest manifest = ManifestParser::parse(json);

    EXPECT_EQ("com.example.test", manifest.symbolicName);
    EXPECT_EQ(Version(1, 0, 0), manifest.version);
}

TEST(ManifestParserTest, Validate) {
    ModuleManifest manifest;
    manifest.symbolicName = "com.example.test";
    manifest.version = Version(1, 0, 0);

    EXPECT_TRUE(ManifestParser::validate(manifest));
}

TEST(ManifestParserTest, ValidateFailsWithoutSymbolicName) {
    ModuleManifest manifest;
    manifest.version = Version(1, 0, 0);

    EXPECT_THROW(ManifestParser::validate(manifest), std::runtime_error);
}

TEST(ManifestParserTest, ValidateFailsWithoutVersion) {
    ModuleManifest manifest;
    manifest.symbolicName = "com.example.test";
    // version defaults to 0.0.0, which is valid

    EXPECT_TRUE(ManifestParser::validate(manifest));
}
