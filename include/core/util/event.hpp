#pragma once

#include <algorithm>
#include <any>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

template <typename... Args, typename F>
std::function<void(const std::vector<std::any> &)> makeCallback(F &&f) {
  return [func = std::forward<F>(f)](const std::vector<std::any> &args) {
    if (args.size() != sizeof...(Args)) {
      throw std::runtime_error("Argument count mismatch");
    }

    auto unpack = [&](auto i) {
      using T = std::tuple_element_t<i, std::tuple<Args...>>;
      return std::any_cast<T>(args[i]);
    };

    std::apply(func, [&]<std::size_t... I>(std::index_sequence<I...>) {
      return std::make_tuple(
          unpack(std::integral_constant<std::size_t, I>{})...);
    }(std::index_sequence_for<Args...>{}));
  };
}

/// @brief Basic event.
class Event {
public:
  virtual ~Event() = default;
};

class EventDispatcher {
private:
  using Callback = std::function<void(const Event &)>;
  std::unordered_map<std::string, std::vector<Callback>> listeners;

public:
  void subscribe(const std::string &eventType, Callback callback) {
    listeners[eventType].push_back(std::move(callback));
  }

  void dispatch(const std::string &eventType, const Event &event) {
    auto it = listeners.find(eventType);
    if (it == listeners.end()) return;
    // a callback could subscribe a new listener for the same
    // eventType while we're dispatching (though unlikely af), which would
    // otherwise resize the vector we're iterating.
    std::vector<Callback> snapshot = it->second;
    for (const auto &callback : snapshot) {
      callback(event);
    }
  }
};

class EventEmitter {
private:
  using Callback = std::function<void(const std::vector<std::any> &)>;
  std::unordered_map<std::string, std::vector<std::pair<size_t, Callback>>>
      listeners;

  size_t nextListenerId = 0;

public:
  using EventId = size_t;

  template <typename... Args>
  EventId connect(const std::string &eventName,
                  std::function<void(Args...)> callback) {
    EventId id = nextListenerId++;

    auto wrapper = [callback](const std::vector<std::any> &args) {
      if constexpr (sizeof...(Args) == 0) {
        callback();
      } else {
        if (args.size() != sizeof...(Args)) {
          throw std::runtime_error("Event argument mismatch");
        }
        std::apply(callback, unpackArguments<Args...>(
                                 args, std::index_sequence_for<Args...>{}));
      }
    };

    listeners[eventName].push_back({id, wrapper});
    return id;
  }

  template <typename... Args>
  EventId connectOnce(const std::string &eventName,
                      std::function<void(Args...)> callback) {
    EventId id = nextListenerId++;

    auto wrapper = [this, eventName, id,
                    callback](const std::vector<std::any> &args) {
      if constexpr (sizeof...(Args) == 0) {
        callback();
      } else {
        if (args.size() != sizeof...(Args)) {
          throw std::runtime_error("Event argument mismatch!");
        }
        std::apply(callback, unpackArguments<Args...>(
                                 args, std::index_sequence_for<Args...>{}));
      }
      this->disconnect(eventName, id);
    };

    listeners[eventName].emplace_back(id, std::move(wrapper));
    return id;
  }

  template <typename... Args>
  void fire(const std::string &eventName, Args &&...args) {
    auto it = listeners.find(eventName);
    if (it == listeners.end() || it->second.empty()) return;

    std::vector<std::any> packedArgs = {std::forward<Args>(args)...};

    // snapshot the listener list before invoking anything, as a listener may
    // call disconnect() on itself during this loop, which erases from the live
    // vector. Iterating that vector directly while it's being mutated is UB
    std::vector<std::pair<size_t, Callback>> snapshot = it->second;
    for (const auto &callback : snapshot) {
      callback.second(packedArgs);
    }
  }

  void disconnect(const std::string &eventName, EventId listenerId) {
    auto it = listeners.find(eventName);
    if (it == listeners.end()) return;
    auto &vec = it->second;
    std::erase_if(
        vec, [listenerId](const auto &p) { return p.first == listenerId; });
  }

  void reset(const std::string &eventName) { listeners.erase(eventName); }

private:
  template <typename... Args, std::size_t... I>
  static std::tuple<Args...> unpackArguments(const std::vector<std::any> &args,
                                             std::index_sequence<I...>) {
    return std::make_tuple(std::any_cast<Args>(args[I])...);
  }
};