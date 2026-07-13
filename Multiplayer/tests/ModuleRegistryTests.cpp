#include <gtest/gtest.h>

#include <memory>

#include "stalkermp/core/ModuleRegistry.h"

using namespace stalkermp::core;

namespace
{
    class TestModule final : public IModule
    {
    public:
        [[nodiscard]] std::string_view Name() const noexcept override { return "World"; }
        [[nodiscard]] SemanticVersion ModuleVersion() const noexcept override { return {1, 0, 0}; }
        [[nodiscard]] Expected<void> Initialize() override { return Success(); }
        void Shutdown() noexcept override {}
    };
} // namespace

TEST(ModuleRegistry, DeclareAndQuery)
{
    ModuleRegistry registry;
    ASSERT_TRUE(registry.Declare({"World", "test module"}).HasValue());

    EXPECT_TRUE(registry.IsDeclared("World"));
    EXPECT_FALSE(registry.IsDeclared("Missing"));
    EXPECT_FALSE(registry.IsImplemented("World"));
    EXPECT_EQ(registry.FindImplementation("World"), nullptr);
    EXPECT_EQ(registry.Count(), static_cast<std::size_t>(1));
}

TEST(ModuleRegistry, RejectsDuplicateDeclaration)
{
    ModuleRegistry registry;
    ASSERT_TRUE(registry.Declare({"World", "first"}).HasValue());

    const auto duplicate = registry.Declare({"World", "second"});
    ASSERT_FALSE(duplicate.HasValue());
    EXPECT_TRUE(duplicate.GetError().Code() == ErrorCode::AlreadyExists);
}

TEST(ModuleRegistry, RejectsEmptyName)
{
    ModuleRegistry registry;
    const auto result = registry.Declare({"", "empty"});
    ASSERT_FALSE(result.HasValue());
    EXPECT_TRUE(result.GetError().Code() == ErrorCode::InvalidArgument);
}

TEST(ModuleRegistry, AttachImplementation)
{
    ModuleRegistry registry;
    ASSERT_TRUE(registry.Declare({"World", "test module"}).HasValue());
    ASSERT_TRUE(registry.AttachImplementation("World", std::make_unique<TestModule>()).HasValue());

    EXPECT_TRUE(registry.IsImplemented("World"));
    ASSERT_NE(registry.FindImplementation("World"), nullptr);
    EXPECT_TRUE(registry.FindImplementation("World")->ModuleVersion() == (SemanticVersion{1, 0, 0}));

    // Second attachment is rejected.
    const auto again = registry.AttachImplementation("World", std::make_unique<TestModule>());
    ASSERT_FALSE(again.HasValue());
    EXPECT_TRUE(again.GetError().Code() == ErrorCode::AlreadyExists);

    // Attaching to an undeclared module is rejected.
    const auto undeclared = registry.AttachImplementation("Ghost", std::make_unique<TestModule>());
    ASSERT_FALSE(undeclared.HasValue());
    EXPECT_TRUE(undeclared.GetError().Code() == ErrorCode::NotFound);
}
