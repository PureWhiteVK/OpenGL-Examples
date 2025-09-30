#ifndef RAII_HELPER_H
#define RAII_HELPER_H

#include <functional>

struct DeferHelper {
  std::function<void()> func{};
  ~DeferHelper() { func(); }
};

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)
#define DEFER(block_code)                                                      \
  Defer CONCAT(_defer_, __LINE__) { [&]() block_code }

#endif // RAII_HELPER_H