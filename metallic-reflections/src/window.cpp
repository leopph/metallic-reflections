#include "window.hpp"

namespace refl {
namespace {
[[nodiscard]] auto CALLBACK WindowProc(HWND const hwnd, UINT const msg, WPARAM const wParam,
                                       LPARAM const lParam) -> LRESULT {
  switch (msg) {
  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }
  default: {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  }
}
}

auto WindowDeleter::operator()(HWND const hwnd) const -> void {
  if (hwnd) {
    DestroyWindow(hwnd);
  }
}

auto MakeWindow() -> WindowHandle {
  WNDCLASSW const wnd_class{
    .style = 0,
    .lpfnWndProc = &WindowProc,
    .cbClsExtra = 0,
    .cbWndExtra = 0,
    .hInstance = GetModuleHandleW(nullptr),
    .hIcon = nullptr,
    .hCursor = LoadCursorW(nullptr, IDC_ARROW),
    .hbrBackground = nullptr,
    .lpszMenuName = nullptr,
    .lpszClassName = L"TracyWindowClass",
  };

  if (!RegisterClassW(&wnd_class)) {
    return nullptr;
  }

  return WindowHandle{
    CreateWindowExW(0, wnd_class.lpszClassName, L"Tracy", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wnd_class.hInstance, nullptr)
  };
}
}
