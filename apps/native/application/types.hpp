#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace enchantment_tweaks::app {

using Value =
    std::variant<std::string, int, double, bool, std::filesystem::path>;

using Arguments = std::unordered_map<std::string, Value>;

struct Result {
  bool success;
  std::string message;

  static Result ok(std::string message = {}) {
    return {true, std::move(message)};
  }

  static Result error(std::string message) {
    return {false, std::move(message)};
  }
};

// Argument specification
struct ArgumentSpec {
  std::string name;
  std::string typeName;
  bool required = true;
};

// Function traits, extract the argument list from a lambda/function
template <typename T> struct FunctionTraits;

// Normal function pointer.

template <typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (*)(Args...)> {
  using Return = ReturnType;
  using ArgumentsTuple = std::tuple<Args...>;
};

// const member function
// Non-mutable lambdas have an operator() matching this form
template <typename Class, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (Class::*)(Args...) const> {
  using Return = ReturnType;
  using ArgumentsTuple = std::tuple<Args...>;
};

// mutable lambda / non-const functor
template <typename Class, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (Class::*)(Args...)> {
  using Return = ReturnType;
  using ArgumentsTuple = std::tuple<Args...>;
};

// Generic callable
template <typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

// debug
template <typename T> std::string typeName() { return "unknown"; }

template <> inline std::string typeName<std::string>() { return "string"; }

template <> inline std::string typeName<int>() { return "int"; }

template <> inline std::string typeName<double>() { return "double"; }

template <> inline std::string typeName<bool>() { return "bool"; }

template <> inline std::string typeName<std::filesystem::path>() {
  return "path";
}

// Remove const/reference before looking up type names
template <typename T> std::string cleanTypeName() {
  using CleanType = std::remove_cvref_t<T>;

  return typeName<CleanType>();
}

template <typename T>
inline std::optional<std::remove_cvref_t<T>>
getArgument(const Arguments &arguments, const std::string &name) {
  using CleanType = std::remove_cvref_t<T>;

  auto iterator = arguments.find(name);

  if (iterator == arguments.end()) {
    return std::nullopt;
  }

  if (const auto *value = std::get_if<CleanType>(&iterator->second)) {
    return *value;
  }

  return std::nullopt;
}

} // namespace enchantment_tweaks::app