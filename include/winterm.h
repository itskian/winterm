#pragma once

#include <Windows.h>
#include <assert.h>
#include <stdint.h>
#include <iostream>
#include <utility>
#include <string>
#include <memory>


namespace term {

struct vec2 {
  int x = 0, y = 0;
};

// console colors
enum : uint16_t {
  black = 0b0000,

  // primary colors
  blue  = 0b0001,
  green = 0b0010,
  red   = 0b0100,

  // secondary colors
  cyan   = blue | green,
  gold   = red | green,
  purple = red | blue,

  // tertiary color(s)
  white = blue | green | red,

  // color modifier
  intense = 0b1000
};

enum option {
  cursor,       // show/hide the cursor
  highlighting  // enable/hide text highlighting
};

struct attribute {
  attribute(uint16_t const f, uint16_t const b = black)
    : foreground(f), background(b), _other(0) {}

  uint16_t foreground : 4;
  uint16_t background : 4;
  uint16_t _other : 8;
};
static_assert(sizeof(attribute) == 2, "attribute is wrong size");

// setup the console
void initialize();

// write the backbuffer to the console window
void flush();

// resize the console window and clear the backbuffer
void size(vec2 const& size);

// get the size of the console (measured in characters)
inline vec2 size();

// get the console window title
std::wstring title();

// set the console window title
void title(wchar_t const* str);

// move the cursor to a specific position
void move_cursor(vec2 const& position);

// get the position of the mouse relative to the console window
// this is measured in characters, not pixels
vec2 mouse_position();

// enable a console option
void enable(option opt);

// disable a console option
void disable(option opt);

// is this console option currently enabled?
bool enabled(option opt);

// empty the input buffer
void reset_input();

// set the input color (when someone types in console)
void input_color(attribute attrib);

// shorthand for fill({ black, black }, L' ');
void clear();

// fill the console with a single character
void fill(attribute attrib, wchar_t c);

// render a horizontal line
void hline(int ypos, attribute attrib, wchar_t c);

// render a vertical line
void vline(int xpos, attribute attrib, wchar_t c);

// render a single character to the console
void character(vec2 const& position, attribute attrib, wchar_t c);

// render a string to the console
// returns the start and end position of the string
template <typename ...Args>
std::pair<int, int> string(vec2 const& position, 
    attribute attrib, wchar_t const* format, Args&& ...args);

// render a horizontally centered string to the console
// returns the start and end position of the string
template <typename ...Args>
std::pair<int, int> stringc(vec2 const& position, 
    attribute attrib, wchar_t const* format, Args&& ...args);

// the length of a string after formatting is applied
template <typename ...Args>
size_t string_len(wchar_t const* format, Args&& ...args);

// get user input
template <typename T>
bool input(vec2 const& position, T& value);


//
//
// implmentation below
//
//


namespace impl {

// stores the current state of the console
inline auto& state() {
  struct {

    // handle to the win32 console
    HANDLE out_handle = nullptr,
      in_handle = nullptr;

    // this is the size of our console in characters, not pixels
    vec2 size = { 0, 0 };

    // an array of characters that will be written to the console all at once
    // to improve performance and reduce tearing
    std::unique_ptr<CHAR_INFO[]> backbuffer;

  } static s;

  return s;
}

// a string's true length after color formatting has been removed
inline size_t string_length(wchar_t const* const str) {
  auto const size = lstrlenW(str);
  size_t real_length = 0;

  for (int i = 0; i < size; ++i) {
    // escape the # if it's prefixed by a backslash
    if (i + 1 < size && str[i] == L'\\' && str[i + 1] == L'#')
      i += 1;
    else if (i + 2 < size && str[i] == L'#') {
      i += 2;
      continue;
    }

    real_length += 1;
  }

  return real_length;
}

// render a string to the console
inline std::pair<int, int> string(vec2 const& position, attribute attrib,
    bool const centered, wchar_t const* const str) {
  assert(position.x >= 0 && position.y >= 0);
  assert(position.y < state().size.y);

  // number of characters in the string (but not necessarily the number of 
  // characters that will be drawn)
  auto const size = lstrlenW(str);

  // first character index
  auto start = (size_t)position.x + (size_t)position.y * state().size.x;

  // apply centering if requested
  if (centered) {
    auto const len = string_length(str);
    if (len / 2 > (size_t)position.x)
      start -= position.x;
    else
      start -= len / 2;
  }

  int xpos = 0;

  // render each character in the string
  for (int i = 0; i < size; ++i) {
    // we reached the end
    if (position.x + xpos >= state().size.x)
      break;

    // escape the # if it's prefixed by a backslash
    if (i + 1 < size && str[i] == L'\\' && str[i + 1] == L'#')
      continue;
    // change the attribute
    else if (i + 2 < size && str[i] == L'#') {
      // foreground
      if (str[i + 1] != L'X')
        attrib.foreground = (uint16_t)(str[i + 1] - L'0');

      // background
      if (str[i + 2] != L'X')
        attrib.background = (uint16_t)(str[i + 2] - L'0');

      // skip the next two color codes
      i += 2;
      continue;
    }

    state().backbuffer[start + xpos] = {
      str[i], *(uint16_t*)&attrib
    };

    xpos += 1;
  }

  auto const first = (int)start - (position.y * state().size.x);
  return { first, first + (int)xpos - 1 };
}

// hide the blinking cursor
inline void disable_cursor() {
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(state().out_handle, &info);

  info.bVisible = false;
  SetConsoleCursorInfo(state().out_handle, &info);
}

// unhide (show) the blinking cursor
inline void enable_cursor() {
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(state().out_handle, &info);

  info.bVisible = true;
  SetConsoleCursorInfo(state().out_handle, &info);
}

// is the cursor visible?
inline bool cursor_enabled() {
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(state().out_handle, &info);
  return info.bVisible;
}

// make it so you cant highlight stuff in the console
inline void disable_highlighting() {
  DWORD mode;
  GetConsoleMode(state().in_handle, &mode);
  SetConsoleMode(impl::state().in_handle,
    (mode & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);
}

// enable highlighting
inline void enable_highlighting() {
  DWORD mode;
  GetConsoleMode(state().in_handle, &mode);
  SetConsoleMode(impl::state().in_handle, 
    (mode | ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);
}

// is highlighting currently enabled
inline bool highlighting_enabled() {
  DWORD mode;
  GetConsoleMode(state().in_handle, &mode);
  return (mode & ENABLE_EXTENDED_FLAGS) && (mode & ENABLE_QUICK_EDIT_MODE);
}

} // namespace impl

// setup the console
inline void initialize() {
  impl::state().out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  impl::state().in_handle = GetStdHandle(STD_INPUT_HANDLE);

  CONSOLE_SCREEN_BUFFER_INFO info;
  GetConsoleScreenBufferInfo(impl::state().out_handle, &info);

  // seems kinda reduntant, but basically just removes the scrollbar
  size({ info.srWindow.Right + 1, info.srWindow.Bottom + 1 });

  auto const window = GetConsoleWindow();

  // prevent resizing the console window
  SetWindowLong(window, GWL_STYLE, GetWindowLong(
    window, GWL_STYLE) & ~(WS_MAXIMIZEBOX | WS_SIZEBOX));
}

// write the backbuffer to the console window
inline void flush() {
  SMALL_RECT region{
    0, 0, (short)impl::state().size.x, (short)impl::state().size.y
  };

  // write to console
  WriteConsoleOutput(
    impl::state().out_handle,
    impl::state().backbuffer.get(),
    { region.Right, region.Bottom },
    { 0, 0 }, &region);
}

// resize the console window and clear the backbuffer
inline void size(vec2 const& size) {
  impl::state().size = size;

  auto const num_chars = (size_t)size.x * (size_t)size.y;
  assert(num_chars > 0);

  // allocate the backbuffer
  impl::state().backbuffer = std::make_unique<CHAR_INFO[]>(num_chars);

  // zero memory
  memset(impl::state().backbuffer.get(), 0, num_chars * sizeof(CHAR_INFO));

  SMALL_RECT const rect{
    0, 0, (short)size.x - 1, (short)size.y - 1
  };

  // the following code is super retarded but it's needed (i think)
  // just read the remarks section in the following pages
  // https://docs.microsoft.com/en-us/windows/console/setconsolewindowinfo
  // https://docs.microsoft.com/en-us/windows/console/setconsolescreenbuffersize

  CONSOLE_SCREEN_BUFFER_INFO info;
  GetConsoleScreenBufferInfo(impl::state().out_handle, &info);

  // too wide
  if (rect.Right > info.dwMaximumWindowSize.X)
    SetConsoleScreenBufferSize(impl::state().out_handle, { (short)size.x, info.dwSize.Y });

  GetConsoleScreenBufferInfo(impl::state().out_handle, &info);

  // too tall
  if (rect.Bottom > info.dwMaximumWindowSize.Y)
    SetConsoleScreenBufferSize(impl::state().out_handle, { info.dwSize.X, (short)size.y });

  // resize the actual console window
  SetConsoleWindowInfo(impl::state().out_handle, TRUE, &rect);
  SetConsoleScreenBufferSize(impl::state().out_handle, { (short)size.x, (short)size.y });
}

// get the size of the console (measured in characters)
inline vec2 size() {
  return impl::state().size;
}

// get the console window title
inline std::wstring title() {
  wchar_t buffer[512] = { 0 };
  GetConsoleTitleW(buffer, sizeof(buffer) / sizeof(wchar_t));
  return buffer;
}

// set the console window title
inline void title(wchar_t const* const str) {
  SetConsoleTitleW(str);
}

// move the cursor to a specific position
inline void move_cursor(vec2 const& position) {
  SetConsoleCursorPosition(impl::state().out_handle,
    { (short)position.x, (short)position.y });
}

// get the position of the mouse relative to the console window
// this is measured in characters, not pixels
inline vec2 mouse_position() {
  POINT point;
  GetCursorPos(&point);

  // adjust the cursor position to be relative to the console window
  ScreenToClient(GetConsoleWindow(), &point);

  // get the font size
  CONSOLE_FONT_INFO info;
  GetCurrentConsoleFont(impl::state().out_handle, FALSE, &info);

  return { point.x / info.dwFontSize.X, point.y / info.dwFontSize.Y };
}

// empty the input buffer
inline void reset_input() {
  FlushConsoleInputBuffer(impl::state().in_handle);
}

// enable a console option
inline void enable(option const opt) {
  switch (opt) {
  case cursor:
    impl::enable_cursor();
    break;
  case highlighting:
    impl::enable_highlighting();
    break;
  }
}

// disable a console option
inline void disable(option const opt) {
  switch (opt) {
  case cursor:
    impl::disable_cursor();
    break;
  case highlighting:
    impl::disable_highlighting();
    break;
  }
}

// is this console option currently enabled?
inline bool enabled(option const opt) {
  switch (opt) {
  case cursor:
    return impl::cursor_enabled();
  case highlighting:
    return impl::highlighting_enabled();
  }

  return false;
}

// set the input color (when someone types in console)
inline void input_color(attribute const attrib) {
  SetConsoleTextAttribute(impl::state().out_handle, *(uint16_t*)&attrib);
}

// shorthand for fill({ black, black }, L' ');
inline void clear() {
  fill({ black, black }, L' ');
}

// fill the console with a single character
inline void fill(attribute const attrib, wchar_t const c) {
  // loop through every character and assign it our attribute and char
  for (size_t i = 0; i <
      (size_t)impl::state().size.x * (size_t)impl::state().size.y; ++i) {
    impl::state().backbuffer[i] = {
      c, *(uint16_t*)&attrib
    };
  }
}

// render a horizontal line
inline void hline(int const ypos, attribute const attrib, wchar_t const c) {
  for (int i = 0; i < impl::state().size.x; ++i) {
    impl::state().backbuffer[i + (size_t)ypos * impl::state().size.x] = {
      c, *(uint16_t*)&attrib
    };
  }
}

// render a vertical line
inline void vline(int const xpos, attribute const attrib, wchar_t const c) {
  for (int i = 0; i < impl::state().size.y; ++i) {
    impl::state().backbuffer[xpos + i * impl::state().size.x] = {
      c, *(uint16_t*)&attrib
    };
  }
}

// render a single character to the console
inline void character(vec2 const& position, attribute const attrib, wchar_t const c) {
  // the index of this position in the character array
  auto const index = position.x + position.y * impl::state().size.x;

  // make sure we're in bounds
  assert(position.x >= 0 && position.x < impl::state().size.x);
  assert(position.y >= 0 && position.y < impl::state().size.y);

  impl::state().backbuffer[index] = {
    c, *(uint16_t*)&attrib
  };
}

// render a string to the console
template <typename ...Args>
inline std::pair<int, int> string(vec2 const& position, attribute const attrib,
    wchar_t const* const format, Args&& ...args) {
  wchar_t buffer[1024];

  // format our string
  auto const swprintf_s_return_value = swprintf_s(
    buffer, format, std::forward<Args>(args)...);

  // maybe the buffer is too small
  assert(swprintf_s_return_value != -1);

  // forward to real function
  return impl::string(position, attrib, false, buffer);
}

// render a horizontally centered string to the console
template <typename ...Args>
inline std::pair<int, int> stringc(vec2 const& position, attribute const attrib,
    wchar_t const* const format, Args&& ...args) {
  wchar_t buffer[1024];

  // format our string
  auto const swprintf_s_return_value = swprintf_s(
    buffer, format, std::forward<Args>(args)...);

  // maybe the buffer is too small
  assert(swprintf_s_return_value != -1);

  // forward to real function
  return impl::string(position, attrib, true, buffer);
}

// the length of a string after formatting is applied
template <typename ...Args>
inline size_t string_len(wchar_t const* const format, Args&& ...args) {
  wchar_t buffer[1024];

  // format our string
  auto const swprintf_s_return_value = swprintf_s(
    buffer, format, std::forward<Args>(args)...);

  // maybe the buffer is too small
  assert(swprintf_s_return_value != -1);

  // forward to real function
  return impl::string_length(buffer);
}

// get user input
template <typename T>
inline bool input(vec2 const& position, T& value) {
  auto const should_hide_cursor = !enabled(cursor);
  bool success = true;

  // make sure we know where to type
  enable(cursor);
  move_cursor(position);

  if constexpr (std::is_same_v<T, std::string>)
    std::getline(std::cin, value);
  else if constexpr (std::is_same_v<T, std::wstring>)
    std::getline(std::wcin, value);
  else {
    // ghetto fix for uint8_t being treated as a char when we really want 
    // it to be treated as an int instead
    if constexpr (std::is_same_v<T, uint8_t>) {
      short tmp;

      success = (bool)(std::cin >> tmp) &&
        tmp >= 0 && tmp < 256;

      value = (uint8_t)tmp;
    }
    else
      success = (bool)(std::cin >> value);

    if (!success) {
      // clear invalid input state
      std::cin.clear();
      std::cin.ignore(INT_MAX, '\n');
    }
  }

  // reset our cursor state basically
  if (should_hide_cursor)
    disable(cursor);

  return success;
}

} // namespace term