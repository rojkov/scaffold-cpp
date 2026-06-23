#include "carrot/common/extension_registry.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace carrot::common {
namespace {

struct TestInterface {
  virtual auto value() -> int = 0;
  virtual ~TestInterface() = default;
  using interface_type = TestInterface;
};

struct FooImpl : TestInterface {
  explicit FooImpl(FactoryContext&, const Config&) {}
  auto value() -> int override { return 42; }
};

struct BarImpl : TestInterface {
  explicit BarImpl(FactoryContext&, const Config&) {}
  auto value() -> int override { return 99; }
};

struct OtherInterface {
  virtual auto name() -> const char* = 0;
  virtual ~OtherInterface() = default;
  using interface_type = OtherInterface;
};

struct BazImpl : OtherInterface {
  explicit BazImpl(FactoryContext&, const Config&) {}
  auto name() -> const char* override { return "baz"; }
};

TEST(ExtensionRegistryTest, CreateRegisteredExtension) {
  auto& reg = ExtensionRegistry::getInstance();
  reg.addFactory<TestInterface>(
      "test_category", "foo",
      [](FactoryContext& ctx, const Config& cfg) -> std::unique_ptr<TestInterface> {
        return std::make_unique<FooImpl>(ctx, cfg);
      });

  FactoryContext ctx(*static_cast<event::Dispatcher*>(nullptr));
  auto instance = reg.create<TestInterface>("test_category", "foo", ctx, {});
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->value(), 42);
}

TEST(ExtensionRegistryTest, UnknownCategoryReturnsNull) {
  auto& reg = ExtensionRegistry::getInstance();
  FactoryContext ctx(*static_cast<event::Dispatcher*>(nullptr));
  auto instance = reg.create<TestInterface>("nonexistent", "foo", ctx, {});
  EXPECT_EQ(instance, nullptr);
}

TEST(ExtensionRegistryTest, UnknownNameReturnsNull) {
  auto& reg = ExtensionRegistry::getInstance();
  FactoryContext ctx(*static_cast<event::Dispatcher*>(nullptr));
  auto instance = reg.create<TestInterface>("test_category", "nonexistent", ctx, {});
  EXPECT_EQ(instance, nullptr);
}

TEST(ExtensionRegistryTest, MultipleExtensionsInSameCategory) {
  auto& reg = ExtensionRegistry::getInstance();
  reg.addFactory<TestInterface>(
      "test_category", "bar",
      [](FactoryContext& ctx, const Config& cfg) -> std::unique_ptr<TestInterface> {
        return std::make_unique<BarImpl>(ctx, cfg);
      });
  FactoryContext ctx(*static_cast<event::Dispatcher*>(nullptr));

  auto foo = reg.create<TestInterface>("test_category", "foo", ctx, {});
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->value(), 42);

  auto bar = reg.create<TestInterface>("test_category", "bar", ctx, {});
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(bar->value(), 99);
}

TEST(ExtensionRegistryTest, DifferentCategoriesAreIndependent) {
  auto& reg = ExtensionRegistry::getInstance();
  reg.addFactory<OtherInterface>(
      "other_category", "baz",
      [](FactoryContext& ctx, const Config& cfg) -> std::unique_ptr<OtherInterface> {
        return std::make_unique<BazImpl>(ctx, cfg);
      });
  FactoryContext ctx(*static_cast<event::Dispatcher*>(nullptr));

  auto baz = reg.create<OtherInterface>("other_category", "baz", ctx, {});
  ASSERT_NE(baz, nullptr);
  EXPECT_STREQ(baz->name(), "baz");
}

} // namespace
} // namespace carrot::common
