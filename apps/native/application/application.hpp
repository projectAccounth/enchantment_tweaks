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
    return {commands_, std::move(name)};
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
      return Result::ok();
    });

    command("tryLoadMc")
        .description("Tries to load a Minecraft version")
        .required<std::string>("minecraft_version")
        .handler([](std::string ver) {
          ver = "a";
          return Result::ok();
        });
  }

  void initParsers() {}

  void loop() {
    while (active) {
    }
  }

  void printCommands() const {
    for (const auto &[name, command] : commands_) {
      debug::Logger::info() << name << '\n';
      debug::Logger::info() << "  " << command.description() << '\n';
      for (const auto &argument : command.arguments()) {
        debug::Logger::info()
            << "  " << (argument.required ? "<" : "[") << argument.name << ": "
            << argument.typeName << (argument.required ? ">" : "]") << '\n';
      }

      debug::Logger::info() << '\n';
    }
  }

private:
  std::unordered_map<std::string, Command> commands_;
  bool active = true;
};

} // namespace enchantment_tweaks::app