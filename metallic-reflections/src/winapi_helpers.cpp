#include "winapi_helpers.hpp"

#include <stdexcept>

namespace tracy {
auto ThrowIfFailed(HRESULT const hr) -> void {
  if (FAILED(hr)) {
    throw std::runtime_error{"Error"};
  }
}
}
