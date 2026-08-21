#pragma once

#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "command.hpp"
#include "types.hpp"

namespace enchantment_tweaks::app {

class Backend {
public:
  CommandBuilder command(std::string name) {
    return CommandBuilder(commands_, std::move(name));
  }

  Result invoke(const std::string &name,
                const Arguments &arguments = {}) const {
    auto iterator = commands_.find(name);

    if (iterator == commands_.end()) {
      return Result::error("Unknown command: " + name);
    }

    return iterator->second.invoke(arguments);
  }

  void registerCommands() {
    // ok what the fuck can i do with this
    command("exit").description("Exit program").handler([this]() {
      active = false;
    });
  }

  void initParsers() {}

  void loop() {
    while (active) {
    }
  }

  void printCommands() const {
    for (const auto &[name, command] : commands_) {
      std::cout << name << '\n';
      std::cout << "  " << command.description() << '\n';
      for (const auto &argument : command.arguments()) {
        std::cout << "  " << (argument.required ? "<" : "[") << argument.name
                  << ": " << argument.typeName
                  << (argument.required ? ">" : "]") << '\n';
      }

      std::cout << '\n';
    }
  }

private:
  std::unordered_map<std::string, Command> commands_;
  bool active = true;
};

} // namespace enchantment_tweaks::app