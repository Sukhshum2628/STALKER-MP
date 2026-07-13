#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "stalkermp/core/ServiceRegistry.h"

using namespace stalkermp::core;

namespace
{
    // Records lifecycle calls so tests can verify ordering.
    std::vector<std::string>& CallLog()
    {
        static std::vector<std::string> log;
        return log;
    }

    class TestServiceBase : public IService
    {
    public:
        TestServiceBase(std::string name, std::vector<std::string> dependencies)
            : m_name(std::move(name))
            , m_dependencies(std::move(dependencies))
        {
        }

        [[nodiscard]] std::string_view Name() const noexcept override { return m_name; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return m_dependencies; }

        [[nodiscard]] Expected<void> Initialize() override
        {
            CallLog().push_back("init:" + m_name);
            return Success();
        }

        void Shutdown() noexcept override
        {
            CallLog().push_back("shutdown:" + m_name);
        }

    private:
        std::string m_name;
        std::vector<std::string> m_dependencies;
    };

    class ServiceA final : public TestServiceBase
    {
    public:
        ServiceA() : TestServiceBase("A", {}) {}
    };

    class ServiceB final : public TestServiceBase
    {
    public:
        ServiceB() : TestServiceBase("B", {"A"}) {}
    };

    class FailingService final : public TestServiceBase
    {
    public:
        FailingService() : TestServiceBase("Failing", {}) {}

        [[nodiscard]] Expected<void> Initialize() override
        {
            return MakeError(ErrorCode::Internal, "intentional failure");
        }
    };
} // namespace

TEST(ServiceRegistry, RegisterAndFind)
{
    ServiceRegistry registry;
    ASSERT_TRUE(registry.Register(std::make_unique<ServiceA>()).HasValue());

    EXPECT_NE(registry.Find<ServiceA>(), nullptr);
    EXPECT_EQ(registry.Find<ServiceB>(), nullptr);
    EXPECT_NE(registry.FindByName("A"), nullptr);
    EXPECT_EQ(registry.FindByName("Nope"), nullptr);
    EXPECT_EQ(registry.Count(), static_cast<std::size_t>(1));
}

TEST(ServiceRegistry, RejectsDuplicates)
{
    ServiceRegistry registry;
    ASSERT_TRUE(registry.Register(std::make_unique<ServiceA>()).HasValue());

    const auto duplicate = registry.Register(std::make_unique<ServiceA>());
    ASSERT_FALSE(duplicate.HasValue());
    EXPECT_TRUE(duplicate.GetError().Code() == ErrorCode::AlreadyExists);
}

TEST(ServiceRegistry, ValidatesDependencyOrder)
{
    // B depends on A but is registered first: must fail validation.
    ServiceRegistry registry;
    ASSERT_TRUE(registry.Register(std::make_unique<ServiceB>()).HasValue());
    ASSERT_TRUE(registry.Register(std::make_unique<ServiceA>()).HasValue());

    const auto validation = registry.ValidateDependencies();
    ASSERT_FALSE(validation.HasValue());
    EXPECT_TRUE(validation.GetError().Code() == ErrorCode::DependencyMissing);
}

TEST(ServiceRegistry, RejectsMissingDependency)
{
    ServiceRegistry registry;
    ASSERT_TRUE(registry.Register(std::make_unique<ServiceB>()).HasValue());

    const auto validation = registry.ValidateDependencies();
    ASSERT_FALSE(validation.HasValue());
    EXPECT_TRUE(validation.GetError().Code() == ErrorCode::DependencyMissing);
}

TEST(ServiceRegistry, InitializesInOrderAndShutsDownInReverse)
{
    CallLog().clear();
    {
        ServiceRegistry registry;
        ASSERT_TRUE(registry.Register(std::make_unique<ServiceA>()).HasValue());
        ASSERT_TRUE(registry.Register(std::make_unique<ServiceB>()).HasValue());
        ASSERT_TRUE(registry.InitializeAll().HasValue());
        registry.ShutdownAll();
    }

    ASSERT_EQ(CallLog().size(), static_cast<std::size_t>(4));
    EXPECT_EQ(CallLog()[0], std::string("init:A"));
    EXPECT_EQ(CallLog()[1], std::string("init:B"));
    EXPECT_EQ(CallLog()[2], std::string("shutdown:B"));
    EXPECT_EQ(CallLog()[3], std::string("shutdown:A"));
}

TEST(ServiceRegistry, UnwindsOnInitializeFailure)
{
    CallLog().clear();
    ServiceRegistry registry;
    ASSERT_TRUE(registry.Register(std::make_unique<ServiceA>()).HasValue());
    ASSERT_TRUE(registry.Register(std::make_unique<FailingService>()).HasValue());

    const auto result = registry.InitializeAll();
    ASSERT_FALSE(result.HasValue());

    // A initialized, then the failure unwound it.
    ASSERT_EQ(CallLog().size(), static_cast<std::size_t>(2));
    EXPECT_EQ(CallLog()[0], std::string("init:A"));
    EXPECT_EQ(CallLog()[1], std::string("shutdown:A"));
}
