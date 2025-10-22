#pragma once

#include <memory>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace refl {
class Window {
public:
  [[nodiscard]] static auto New() -> std::unique_ptr<Window>;

  [[nodiscard]] auto GetHwnd() const -> HWND;
  [[nodiscard]] auto IsKeyPressed(char key) const -> bool;

  Window(Window const& other) = delete;
  Window(Window&& other) = delete;

  ~Window();

  auto operator=(Window const& other) -> Window& = delete;
  auto operator=(Window&& other) -> Window& = delete;

private:
  explicit Window(HWND hwnd);

  static auto CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) -> LRESULT;

  HWND hwnd_;
  std::unordered_map<char, bool> key_states_;
};
}
