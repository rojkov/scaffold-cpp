#pragma once

#include <any>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace carrot::event {
class Dispatcher;
}

namespace carrot::common {

class FactoryContext {
public:
  explicit FactoryContext(event::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}

  auto dispatcher() -> event::Dispatcher& { return dispatcher_; }

private:
  event::Dispatcher& dispatcher_;
};

using Config = std::any;

class ExtensionRegistry {
public:
  static auto getInstance() -> ExtensionRegistry& {
    static ExtensionRegistry instance;
    return instance;
  }

  template <typename T>
  void addFactory(std::string_view category, std::string_view name,
                  std::function<std::unique_ptr<T>(FactoryContext&, const Config&)> factory) {
    auto erased_factory = [f = std::move(factory)](FactoryContext& ctx,
                                                   const Config& cfg) -> void* {
      return f(ctx, cfg).release();
    };
    factories_[std::string(category)].emplace(
        std::string(name),
        FactoryEntry{std::move(erased_factory), [](void* p) { delete static_cast<T*>(p); },
                     typeid(T)});
  }

  template <typename T>
  auto create(std::string_view category, std::string_view name, FactoryContext& ctx,
              const Config& cfg) -> std::unique_ptr<T> {
    auto cat_key = std::string(category);
    auto cat_it = factories_.find(cat_key);
    if (cat_it == factories_.end()) {
      return nullptr;
    }
    auto name_key = std::string(name);
    auto f_it = cat_it->second.find(name_key);
    if (f_it == cat_it->second.end()) {
      return nullptr;
    }
    auto& entry = f_it->second;
    assert(entry.type == typeid(T));
    auto* ptr = static_cast<T*>(entry.factory(ctx, cfg));
    return std::unique_ptr<T>(ptr);
  }

private:
  ExtensionRegistry() = default;

  struct FactoryEntry {
    std::function<void*(FactoryContext&, const Config&)> factory;
    std::function<void(void*)> deleter;
    std::type_index type;
  };

  std::unordered_map<std::string, std::unordered_map<std::string, FactoryEntry>> factories_;
};

} // namespace carrot::common

#define CARROT_EXTENSION_CONCAT_IMPL(a, b) a##b
#define CARROT_EXTENSION_CONCAT(a, b) CARROT_EXTENSION_CONCAT_IMPL(a, b)

#define CARROT_REGISTER_EXTENSION(category, name, FactoryClass)                    \
  namespace {                                                                      \
  struct CARROT_EXTENSION_CONCAT(Registration_for_##category##_, __LINE__) {       \
    CARROT_EXTENSION_CONCAT(Registration_for_##category##_, __LINE__)() {          \
      ::carrot::common::ExtensionRegistry::getInstance().addFactory<               \
          typename FactoryClass::interface_type>(                                   \
          #category, name,                                                         \
          [](::carrot::common::FactoryContext& ctx,                                \
             const ::carrot::common::Config& cfg)                                  \
              -> std::unique_ptr<typename FactoryClass::interface_type> {          \
            return std::make_unique<FactoryClass>(ctx, cfg);                       \
          });                                                                      \
    }                                                                              \
  } CARROT_EXTENSION_CONCAT(carrot_reg_##category##_, __LINE__);                   \
  } // namespace
