#pragma once

#include <memory>
#include <type_traits>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace tracy {
class WindowDeleter {
public:
  auto operator()(HWND hwnd) const -> void;
};

using WindowHandle = std::unique_ptr<std::remove_pointer_t<HWND>, WindowDeleter>;

[[nodiscard]] auto MakeWindow() -> WindowHandle;
}
