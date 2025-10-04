#include "security/code_verifier.h"
#include <gtest/gtest.h>
#include <fstream>

using namespace cdmf::security;

class CodeVerifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        verifier = std::make_unique<CodeVerifier>();

        // Create test files
        createTestFile("test_module.so", "binary content");
        createTestFile("test_file.txt", "test content");
        createTestSignature("test_module.so.sig", "TestSigner:RSA_SHA256:2025-10-04");
        createTestSignature("test_file.txt.sig", "TestSigner:RSA_SHA256:2025-10-04");

        // Add a trusted signer
        TrustedSigner signer;
        signer.name = "TestSigner";
        signer.email = "test@example.com";
        signer.publicKeyPath = "/path/to/public.key";
        signer.fingerprint = "ABC123DEF456";
        signer.enabled = true;
        verifier->addTrustedSigner(signer);
    }

    void TearDown() override {
        // Clean up test files
        std::remove("test_module.so");
        std::remove("test_module.so.sig");
        std::remove("test_file.txt");
        std::remove("test_file.txt.sig");
        std::remove("test_signers.conf");
    }

    void createTestFile(const std::string& path, const std::string& content) {
        std::ofstream file(path, std::ios::binary);
        file << content;
        file.close();
    }

    void createTestSignature(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    std::unique_ptr<CodeVerifier> verifier;
};

// ========== Trusted Signer Management Tests ==========

TEST_F(CodeVerifierTest, AddTrustedSigner) {
    TrustedSigner signer;
    signer.name = "NewSigner";
    signer.email = "new@example.com";
    signer.publicKeyPath = "/path/to/new.key";
    signer.fingerprint = "XYZ789";
    signer.enabled = true;

    EXPECT_TRUE(verifier->addTrustedSigner(signer));
}

TEST_F(CodeVerifierTest, AddTrustedSignerEmptyFingerprint) {
    TrustedSigner signer;
    signer.name = "InvalidSigner";
    signer.fingerprint = "";

    EXPECT_FALSE(verifier->addTrustedSigner(signer));
}

TEST_F(CodeVerifierTest, AddDuplicateTrustedSigner) {
    TrustedSigner signer;
    signer.name = "TestSigner";
    signer.fingerprint = "ABC123DEF456";
    signer.enabled = true;

    EXPECT_FALSE(verifier->addTrustedSigner(signer)); // Already exists
}

TEST_F(CodeVerifierTest, RemoveTrustedSigner) {
    EXPECT_TRUE(verifier->removeTrustedSigner("ABC123DEF456"));
}

TEST_F(CodeVerifierTest, RemoveNonExistentSigner) {
    EXPECT_FALSE(verifier->removeTrustedSigner("NONEXISTENT"));
}

TEST_F(CodeVerifierTest, GetTrustedSigners) {
    auto signers = verifier->getTrustedSigners();
    EXPECT_GE(signers.size(), 1); // At least the test signer
}

TEST_F(CodeVerifierTest, IsTrustedSigner) {
    EXPECT_TRUE(verifier->isTrustedSigner("ABC123DEF456"));
    EXPECT_FALSE(verifier->isTrustedSigner("UNKNOWN"));
}

TEST_F(CodeVerifierTest, IsTrustedSignerByName) {
    EXPECT_TRUE(verifier->isTrustedSigner("TestSigner"));
}

TEST_F(CodeVerifierTest, SetSignerEnabled) {
    EXPECT_TRUE(verifier->setSignerEnabled("ABC123DEF456", false));
    EXPECT_FALSE(verifier->isTrustedSigner("ABC123DEF456")); // Should be disabled
}

TEST_F(CodeVerifierTest, SetSignerEnabledNonExistent) {
    EXPECT_FALSE(verifier->setSignerEnabled("NONEXISTENT", true));
}

// ========== Configuration Tests ==========

TEST_F(CodeVerifierTest, SetSignatureRequired) {
    verifier->setSignatureRequired(true);
    EXPECT_TRUE(verifier->isSignatureRequired());

    verifier->setSignatureRequired(false);
    EXPECT_FALSE(verifier->isSignatureRequired());
}

TEST_F(CodeVerifierTest, SetVerifyCertificateChain) {
    verifier->setVerifyCertificateChain(false);
    EXPECT_FALSE(verifier->isVerifyCertificateChain());

    verifier->setVerifyCertificateChain(true);
    EXPECT_TRUE(verifier->isVerifyCertificateChain());
}

TEST_F(CodeVerifierTest, SetVerifyTimestamp) {
    verifier->setVerifyTimestamp(false);
    EXPECT_FALSE(verifier->isVerifyTimestamp());

    verifier->setVerifyTimestamp(true);
    EXPECT_TRUE(verifier->isVerifyTimestamp());
}

// ========== Configuration Persistence Tests ==========

TEST_F(CodeVerifierTest, SaveTrustedSigners) {
    EXPECT_TRUE(verifier->saveTrustedSigners("test_signers.conf"));

    std::ifstream file("test_signers.conf");
    EXPECT_TRUE(file.good());
    file.close();
}

TEST_F(CodeVerifierTest, LoadTrustedSigners) {
    // Save first
    verifier->saveTrustedSigners("test_signers.conf");

    // Create new verifier and load
    auto newVerifier = std::make_unique<CodeVerifier>();
    EXPECT_TRUE(newVerifier->loadTrustedSigners("test_signers.conf"));

    auto signers = newVerifier->getTrustedSigners();
    EXPECT_GE(signers.size(), 1);
}

TEST_F(CodeVerifierTest, LoadTrustedSignersNonExistent) {
    EXPECT_FALSE(verifier->loadTrustedSigners("nonexistent.conf"));
}

// ========== Verification Tests ==========

TEST_F(CodeVerifierTest, VerifyModuleSigned) {
    auto info = verifier->verifyModule("test_module.so");

    EXPECT_EQ(VerificationResult::VERIFIED, info.result);
    EXPECT_EQ("TestSigner", info.signerName);
    EXPECT_EQ("RSA_SHA256", info.algorithm);
}

TEST_F(CodeVerifierTest, VerifyModuleNotSigned) {
    std::remove("test_module.so.sig"); // Remove signature

    verifier->setSignatureRequired(false);
    auto info = verifier->verifyModule("test_module.so");

    EXPECT_EQ(VerificationResult::NOT_SIGNED, info.result);
}

TEST_F(CodeVerifierTest, VerifyModuleNotSignedRequired) {
    std::remove("test_module.so.sig"); // Remove signature

    verifier->setSignatureRequired(true);
    auto info = verifier->verifyModule("test_module.so");

    EXPECT_EQ(VerificationResult::NOT_SIGNED, info.result);
}

TEST_F(CodeVerifierTest, VerifyModuleUntrustedSigner) {
    // Create signature with untrusted signer
    createTestSignature("test_module.so.sig", "UntrustedSigner:RSA_SHA256:2025-10-04");

    auto info = verifier->verifyModule("test_module.so");
    EXPECT_EQ(VerificationResult::UNTRUSTED, info.result);
}

TEST_F(CodeVerifierTest, VerifyModuleNonExistent) {
    auto info = verifier->verifyModule("nonexistent.so");
    EXPECT_EQ(VerificationResult::ERROR, info.result);
}

TEST_F(CodeVerifierTest, VerifyFile) {
    auto info = verifier->verifyFile("test_file.txt", "test_file.txt.sig");
    EXPECT_EQ(VerificationResult::VERIFIED, info.result);
}

TEST_F(CodeVerifierTest, VerifyFileNotFound) {
    auto info = verifier->verifyFile("nonexistent.txt", "test_file.txt.sig");
    EXPECT_EQ(VerificationResult::ERROR, info.result);
}

TEST_F(CodeVerifierTest, VerifyFileSignatureNotFound) {
    auto info = verifier->verifyFile("test_file.txt", "nonexistent.sig");
    EXPECT_EQ(VerificationResult::ERROR, info.result);
}

// ========== Hash Computation Tests ==========

TEST_F(CodeVerifierTest, ComputeFileHash) {
    std::string hash = verifier->computeFileHash("test_file.txt", "SHA256");
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(64, hash.length()); // SHA256 produces 64 hex characters
}

TEST_F(CodeVerifierTest, ComputeFileHashNonExistent) {
    std::string hash = verifier->computeFileHash("nonexistent.txt", "SHA256");
    EXPECT_TRUE(hash.empty());
}

TEST_F(CodeVerifierTest, ComputeFileHashConsistent) {
    std::string hash1 = verifier->computeFileHash("test_file.txt", "SHA256");
    std::string hash2 = verifier->computeFileHash("test_file.txt", "SHA256");
    EXPECT_EQ(hash1, hash2);
}

// ========== Verification Result Tests ==========

TEST_F(CodeVerifierTest, VerificationResultToString) {
    EXPECT_EQ("VERIFIED", CodeVerifier::verificationResultToString(VerificationResult::VERIFIED));
    EXPECT_EQ("INVALID_SIGNATURE", CodeVerifier::verificationResultToString(VerificationResult::INVALID_SIGNATURE));
    EXPECT_EQ("UNTRUSTED", CodeVerifier::verificationResultToString(VerificationResult::UNTRUSTED));
    EXPECT_EQ("EXPIRED", CodeVerifier::verificationResultToString(VerificationResult::EXPIRED));
    EXPECT_EQ("REVOKED", CodeVerifier::verificationResultToString(VerificationResult::REVOKED));
    EXPECT_EQ("NOT_SIGNED", CodeVerifier::verificationResultToString(VerificationResult::NOT_SIGNED));
    EXPECT_EQ("ERROR", CodeVerifier::verificationResultToString(VerificationResult::ERROR));
}

// ========== VerificationInfo Tests ==========

TEST_F(CodeVerifierTest, VerificationInfoContainsMetadata) {
    auto info = verifier->verifyModule("test_module.so");

    EXPECT_FALSE(info.signerName.empty());
    EXPECT_FALSE(info.algorithm.empty());
    EXPECT_FALSE(info.errorMessage.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
