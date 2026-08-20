#pragma once

#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "types.hpp"

namespace enchantment_tweaks::app {

class Command {
public:
  using Invoker = std::function<Result(const Arguments &)>;

  Command(std::string name, std::string description,
          std::vector<ArgumentSpec> arguments, Invoker invoker)
      : name_(std::move(name)), description_(std::move(description)),
        arguments_(std::move(arguments)), invoker_(std::move(invoker)) {}

  const std::string &name() const { return name_; }

  const std::string &description() const { return description_; }

  const std::vector<ArgumentSpec> &arguments() const { return arguments_; }

  Result invoke(const Arguments &arguments) const {
    return invoker_(arguments);
  }

private:
  std::string name_;
  std::string description_;

  std::vector<ArgumentSpec> arguments_;

  Invoker invoker_;
};

class CommandBuilder {
public:
  CommandBuilder(std::unordered_map<std::string, Command> &commands,

                 std::string name)
      : commands_(commands), name_(std::move(name)) {}

  CommandBuilder &description(std::string value) {
    description_ = std::move(value);

    return *this;
  }

  template <typename T> CommandBuilder &required(std::string name) {
    arguments_.push_back({.name = std::move(name),
                          .typeName = cleanTypeName<T>(),
                          .required = true});

    return *this;
  }

  template <typename T> CommandBuilder &optional(std::string name) {
    arguments_.push_back({.name = std::move(name),
                          .typeName = cleanTypeName<T>(),
                          .required = false});

    return *this;
  }

  template <typename Function> void handler(Function function) {
    using Traits = FunctionTraits<Function>;

    using ArgumentsTuple = typename Traits::ArgumentsTuple;

    constexpr std::size_t parameterCount = std::tuple_size_v<ArgumentsTuple>;

    if (parameterCount != arguments_.size()) {
      throw std::runtime_error("Number of handler parameters does not "
                               "match number of declared arguments.");
    }

    // Copy the specifications because the lambda stored in, Command must
    // outlive this builder
    auto argumentSpecs = arguments_;

    Command::Invoker invoker =
        [function = std::move(function),
         argumentSpecs = std::move(argumentSpecs)](
            const Arguments &providedArguments) mutable -> Result {
      return invokeFunction<Function, ArgumentsTuple>(function, argumentSpecs,
                                                      providedArguments);
    };

    Command command(std::move(name_), std::move(description_),
                    std::move(arguments_), std::move(invoker));

    commands_.insert_or_assign(command.name(), std::move(command));
  }

private:
  std::unordered_map<std::string, Command> &commands_;

  std::string name_;
  std::string description_;

  std::vector<ArgumentSpec> arguments_;

  template <typename Function, typename Tuple, std::size_t... Index>
  static Result invokeFunctionImpl(Function &function,
                                   const std::vector<ArgumentSpec> &specs,
                                   const Arguments &providedArguments,
                                   std::index_sequence<Index...>) {
    using ValuesTuple = std::tuple<std::optional<
        std::remove_cvref_t<std::tuple_element_t<Index, Tuple>>>...>;

    ValuesTuple values{getArgument<std::tuple_element_t<Index, Tuple>>(
        providedArguments, specs[Index].name)...};

    bool valid = true;

    std::string errorMessage;

    // fuck you formatter

    (
        [&] {
          if (!std::get<Index>(values)) {
            if (specs[Index].required) {
              valid = false;

              errorMessage =
                  "Missing or invalid argument: " + specs[Index].name;
            }
          }
        }(),
        ...);

    if (!valid) {
      return Result::error(std::move(errorMessage));
    }

    // Currently optional arguments are represented by optional<T> handler
    // parameters
    return function(extractValue<std::tuple_element_t<Index, Tuple>>(
        std::get<Index>(values))...);
  }

  template <typename Function, typename Tuple>
  static Result invokeFunction(Function &function,

                               const std::vector<ArgumentSpec> &specs,

                               const Arguments &providedArguments) {
    return invokeFunctionImpl<Function, Tuple>(
        function, specs, providedArguments,

        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
  }

  template <typename T>
  static decltype(auto)
  extractValue(const std::optional<std::remove_cvref_t<T>> &value) {
    return *value;
  }
};

} // namespace enchantment_tweaks::app