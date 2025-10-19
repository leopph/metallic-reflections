#include "winapi_helpers.hpp"

import std;

namespace refl {
auto ThrowIfFailed(HRESULT const hr) -> void {
  if (FAILED(hr)) {
    throw std::runtime_error{"Error"};
  }
}
}
