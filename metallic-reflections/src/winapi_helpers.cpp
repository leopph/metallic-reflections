#include "winapi_helpers.hpp"

#include <stdexcept>

namespace refl {
auto ThrowIfFailed(HRESULT const hr) -> void {
  if (FAILED(hr)) {
    throw std::runtime_error{"Error"};
  }
}
}
