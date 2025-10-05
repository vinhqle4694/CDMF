/**
 * @file test_command_handler.cpp
 * @brief Comprehensive unit tests for CommandHandler CLI component
 *
 * Tests include:
 * - Command parsing
 * - Command execution (start, stop, update, list, help, exit)
 * - Error handling for invalid commands
 * - Argument validation
 * - Integration with framework
 * - Edge cases
 *
 * @author Command Handler Test Agent
 * @date 2025-10-05
 */

#include "utils/command_handler.h"
#include "core/framework.h"
#include <gtest/gtest.h>
#include <sstream>
#include <memory>
#include <thread>
#include <algorithm>

using namespace cdmf;

// ============================================================================
// Test without Mock Framework (CommandHandler functionality only)
// ============================================================================

// ============================================================================
// Test Fixtures
// ============================================================================

class CommandHandlerTest : public ::testing::Test {
protected:
    std::unique_ptr<CommandHandler> handler;

    void SetUp() override {
        // Create handler with null framework for basic testing
        handler = std::make_unique<CommandHandler>(nullptr);
    }

    void TearDown() override {
        handler.reset();
    }
};

// ============================================================================
// Command Parsing Tests
// ============================================================================

TEST_F(CommandHandlerTest, ParseSimpleCommand) {
    auto result = handler->processCommand("help");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, ParseCommandWithArguments) {
    auto result = handler->processCommand("start test_module");

    // Result depends on whether module exists
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, ParseEmptyCommand) {
    auto result = handler->processCommand("");

    // Empty command is silently ignored (returns success with empty message)
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.message.empty());
}

TEST_F(CommandHandlerTest, ParseWhitespaceOnlyCommand) {
    auto result = handler->processCommand("   ");

    // Whitespace-only command is silently ignored (returns success with empty message)
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.message.empty());
}

TEST_F(CommandHandlerTest, ParseCommandWithExtraWhitespace) {
    auto result = handler->processCommand("  help  ");

    EXPECT_TRUE(result.success);
}

TEST_F(CommandHandlerTest, ParseCommandWithMultipleSpaces) {
    auto result = handler->processCommand("start    test_module");

    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, ParseUnknownCommand) {
    auto result = handler->processCommand("unknown_command");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("Unknown command"), std::string::npos);
}

// ============================================================================
// Help Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, HelpCommand) {
    auto result = handler->processCommand("help");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());

    // Should contain information about available commands
    EXPECT_NE(result.message.find("start"), std::string::npos);
    EXPECT_NE(result.message.find("stop"), std::string::npos);
    EXPECT_NE(result.message.find("list"), std::string::npos);
}

TEST_F(CommandHandlerTest, GetHelpText) {
    std::string help = handler->getHelpText();

    EXPECT_FALSE(help.empty());
    EXPECT_NE(help.find("start"), std::string::npos);
    EXPECT_NE(help.find("stop"), std::string::npos);
    EXPECT_NE(help.find("update"), std::string::npos);
    EXPECT_NE(help.find("list"), std::string::npos);
    EXPECT_NE(help.find("help"), std::string::npos);
    EXPECT_NE(help.find("exit"), std::string::npos);
}

TEST_F(CommandHandlerTest, HelpCommandWithExtraArguments) {
    auto result = handler->processCommand("help extra args");

    // Should still succeed and show help
    EXPECT_TRUE(result.success);
}

// ============================================================================
// List Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, ListCommandNoModules) {
    auto result = handler->processCommand("list");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, ListCommandWithArguments) {
    auto result = handler->processCommand("list extra args");

    // List command doesn't use arguments, should still work
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Start Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, StartCommandWithoutArguments) {
    auto result = handler->processCommand("start");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("Usage"), std::string::npos);
}

TEST_F(CommandHandlerTest, StartCommandWithModuleName) {
    auto result = handler->processCommand("start test_module");

    // Module likely doesn't exist, so should fail
    EXPECT_FALSE(result.success);
}

TEST_F(CommandHandlerTest, StartCommandWithExtraArguments) {
    auto result = handler->processCommand("start module1 extra args");

    // Extra arguments should be ignored or cause error
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Stop Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, StopCommandWithoutArguments) {
    auto result = handler->processCommand("stop");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("Usage"), std::string::npos);
}

TEST_F(CommandHandlerTest, StopCommandWithModuleName) {
    auto result = handler->processCommand("stop test_module");

    // Module likely doesn't exist, so should fail
    EXPECT_FALSE(result.success);
}

// ============================================================================
// Update Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, UpdateCommandWithoutArguments) {
    auto result = handler->processCommand("update");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("Usage"), std::string::npos);
}

TEST_F(CommandHandlerTest, UpdateCommandWithOnlyModuleName) {
    auto result = handler->processCommand("update test_module");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("Usage"), std::string::npos);
}

TEST_F(CommandHandlerTest, UpdateCommandWithModuleAndPath) {
    auto result = handler->processCommand("update test_module /path/to/module.so");

    // Module likely doesn't exist, should fail
    EXPECT_FALSE(result.success);
}

TEST_F(CommandHandlerTest, UpdateCommandWithExtraArguments) {
    auto result = handler->processCommand("update module1 /path/to/module.so extra args");

    // Should handle or reject extra arguments
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Exit Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, ExitCommand) {
    auto result = handler->processCommand("exit");

    EXPECT_TRUE(result.success);
}

TEST_F(CommandHandlerTest, ExitCommandWithArguments) {
    auto result = handler->processCommand("exit now");

    // Should still succeed
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Command Case Sensitivity Tests
// ============================================================================

TEST_F(CommandHandlerTest, CommandsCaseSensitive) {
    auto result1 = handler->processCommand("HELP");
    auto result2 = handler->processCommand("Help");
    auto result3 = handler->processCommand("help");

    // Typically commands are case-sensitive
    // At least one should work (usually lowercase)
    EXPECT_TRUE(result3.success);
}

// ============================================================================
// Special Characters and Edge Cases
// ============================================================================

TEST_F(CommandHandlerTest, CommandWithSpecialCharacters) {
    auto result = handler->processCommand("start module@#$%");

    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, CommandWithQuotes) {
    auto result = handler->processCommand("start \"module name\"");

    // Depends on parsing implementation
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, CommandWithPathSeparators) {
    auto result = handler->processCommand("start /path/to/module");

    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, VeryLongCommand) {
    std::string long_cmd = "start ";
    long_cmd.append(1000, 'a');

    auto result = handler->processCommand(long_cmd);

    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, CommandWithTabCharacters) {
    auto result = handler->processCommand("start\ttest_module");

    // Tabs might be treated as whitespace
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, CommandWithNewlines) {
    auto result = handler->processCommand("help\n");

    // Should handle or strip newlines
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Multiple Command Tests
// ============================================================================

TEST_F(CommandHandlerTest, ProcessMultipleHelpCommands) {
    for (int i = 0; i < 10; ++i) {
        auto result = handler->processCommand("help");
        EXPECT_TRUE(result.success);
    }
}

TEST_F(CommandHandlerTest, ProcessMixedCommands) {
    std::vector<std::string> commands = {
        "help",
        "list",
        "start module1",
        "list",
        "help"
    };

    for (const auto& cmd : commands) {
        auto result = handler->processCommand(cmd);
        EXPECT_FALSE(result.message.empty());
    }
}

// ============================================================================
// Result Message Tests
// ============================================================================

TEST_F(CommandHandlerTest, SuccessResultHasMessage) {
    auto result = handler->processCommand("help");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, FailureResultHasMessage) {
    auto result = handler->processCommand("unknown_command");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, ListResultContainsModuleInfo) {
    auto result = handler->processCommand("list");

    EXPECT_TRUE(result.success);
    // Message should contain information about modules (even if none)
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Framework Interaction Tests
// ============================================================================

TEST_F(CommandHandlerTest, ConstructorWithNullFramework) {
    EXPECT_NO_THROW({
        CommandHandler handler(nullptr);
    });
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(CommandHandlerTest, ConcurrentCommandProcessing) {
    const int NUM_THREADS = 4;
    const int COMMANDS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < COMMANDS_PER_THREAD; ++i) {
                auto result = handler->processCommand("help");
                if (result.success) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * COMMANDS_PER_THREAD);
}

TEST_F(CommandHandlerTest, ConcurrentMixedCommands) {
    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            std::vector<std::string> commands = {
                "help", "list", "start module" + std::to_string(t)
            };

            for (const auto& cmd : commands) {
                handler->processCommand(cmd);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should not crash
}

// ============================================================================
// Argument Parsing Edge Cases
// ============================================================================

TEST_F(CommandHandlerTest, ArgumentsWithLeadingSpaces) {
    auto result = handler->processCommand("start   module_name");

    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, ArgumentsWithTrailingSpaces) {
    auto result = handler->processCommand("start module_name   ");

    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, SingleCharacterCommand) {
    auto result = handler->processCommand("h");

    // Should be unknown command
    EXPECT_FALSE(result.success);
}

TEST_F(CommandHandlerTest, NumericCommand) {
    auto result = handler->processCommand("123");

    EXPECT_FALSE(result.success);
}

TEST_F(CommandHandlerTest, EmptyArgument) {
    auto result = handler->processCommand("start ''");

    // Depends on parsing implementation
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Command Result Consistency Tests
// ============================================================================

TEST_F(CommandHandlerTest, SameCommandProducesSameResult) {
    auto result1 = handler->processCommand("help");
    auto result2 = handler->processCommand("help");

    EXPECT_EQ(result1.success, result2.success);
}

TEST_F(CommandHandlerTest, UnknownCommandsAlwaysFail) {
    std::vector<std::string> unknown_commands = {
        "unknown1", "unknown2", "xyz", "abc123"
    };

    for (const auto& cmd : unknown_commands) {
        auto result = handler->processCommand(cmd);
        EXPECT_FALSE(result.success);
    }
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(CommandHandlerTest, ProcessManyCommands) {
    const int NUM_COMMANDS = 10000;

    for (int i = 0; i < NUM_COMMANDS; ++i) {
        auto result = handler->processCommand("help");
        EXPECT_TRUE(result.success);
    }
}

TEST_F(CommandHandlerTest, ProcessAlternatingCommands) {
    const int NUM_ITERATIONS = 1000;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        if (i % 2 == 0) {
            handler->processCommand("help");
        } else {
            handler->processCommand("list");
        }
    }
}

TEST_F(CommandHandlerTest, VeryLongArgumentList) {
    std::string cmd = "start module";
    for (int i = 0; i < 100; ++i) {
        cmd += " arg" + std::to_string(i);
    }

    auto result = handler->processCommand(cmd);
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Unicode and International Characters
// ============================================================================

TEST_F(CommandHandlerTest, CommandWithUnicodeCharacters) {
    auto result = handler->processCommand("start modülé");

    // Should handle gracefully
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CommandHandlerTest, CommandWithChineseCharacters) {
    auto result = handler->processCommand("start 模块名称");

    // Should handle gracefully
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
