#include "window.hpp"

namespace refl {
auto Window::New() -> std::unique_ptr<Window> {
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

  auto const hwnd{
    CreateWindowExW(0, wnd_class.lpszClassName, L"Tracy", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wnd_class.hInstance, nullptr)
  };

  if (!hwnd) {
    return nullptr;
  }

  std::unique_ptr<Window> wnd{new Window{hwnd}};

  if (!wnd) {
    DestroyWindow(hwnd);
    return nullptr;
  }

  SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(wnd.get()));

  return wnd;
}

auto Window::GetHwnd() const -> HWND {
  return hwnd_;
}

auto Window::IsKeyPressed(char const key) const -> bool {
  if (auto const it{key_states_.find(key)}; it != key_states_.end()) {
    return it->second;
  }

  return false;
}

Window::~Window() {
  DestroyWindow(hwnd_);
}

Window::Window(HWND const hwnd) : hwnd_{hwnd} {
}

auto Window::WindowProc(HWND const hwnd, UINT const msg, WPARAM const w_param, LPARAM const l_param) -> LRESULT {
  switch (msg) {
  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }

  case WM_KEYDOWN: {
    if (auto const wnd{reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))}) {
      wnd->key_states_[static_cast<char>(w_param)] = true;
      return 0;
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
  }

  case WM_KEYUP: {
    if (auto const wnd{reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))}) {
      wnd->key_states_[static_cast<char>(w_param)] = false;
      return 0;
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
  }

  default: {
    return DefWindowProcW(hwnd, msg, w_param, l_param);
  }
  }
}
}
