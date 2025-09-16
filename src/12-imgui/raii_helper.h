#ifndef RAII_HELPER_H
#define RAII_HELPER_H

#include <functional>

struct ScopeGuard {
  ScopeGuard(std::function<void()> exit_func) : m_exit_func(exit_func) {}
  ~ScopeGuard() { m_exit_func(); }
  std::function<void()> m_exit_func{};
};

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define GUARD_EXIT(func_body)                                                  \
  ScopeGuard CONCAT(scope_guard_, __LINE__)([&]() func_body)

#endif//RAII_HELPER_H