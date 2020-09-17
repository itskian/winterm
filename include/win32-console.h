#pragma once

#include <Windows.h>
#include <assert.h>
#include <stdint.h>
#include <string>
#include <memory>


namespace w32c {

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

struct attribute {
  attribute(uint16_t const f, uint16_t const b = black)
    : foreground(f), background(b), _other(0) {}

  uint16_t foreground : 4;
  uint16_t background : 4;
  uint16_t _other : 8;
};
static_assert(sizeof(attribute) == 2, "attribute is wrong size");

// setup the console
void initialize(vec2 const& size);

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

// hide the blinking cursor
void hide_cursor();

// unhide (show) the blinking cursor
void show_cursor();

// move the cursor to a specific position
void move_cursor(vec2 const& position);

// is the cursor visible?
bool cursor_visible();

// set the input color (when someone types in console)
void input_color(attribute attrib);

// shorthand for fill({ black, black }, L' ');
void clear();

// fill the console with a single character
void fill(attribute attrib, wchar_t c);

// render a single character to the console
void character(vec2 const& position, attribute attrib, wchar_t c);

// render a string to the console
template <typename ...Args>
void string(vec2 const& position, attribute attrib, 
    wchar_t const* format, Args&& ...args);

// render a horizontally centered string to the console
template <typename ...Args>
void stringc(vec2 const& position, attribute const attrib,
    wchar_t const* const format, Args&& ...args);

// the length of a string after formatting is applied
template <typename ...Args>
size_t string_len(wchar_t const* const format, Args&& ...args);

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
struct {

  // handle to the win32 console
  HANDLE handle = nullptr;

  // this is the size of our console in characters, not pixels
  vec2 size = { 0, 0 };

  // an array of characters that will be written to the console all at once
  // to improve performance and reduce tearing
  std::unique_ptr<CHAR_INFO[]> backbuffer;

} state;

// a string's true length after color formatting has been removed
inline size_t string_length(wchar_t const* const str) {
  auto const size = lstrlenW(str);
  size_t real_length = 0;

  for (size_t i = 0; i < size; ++i) {
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
inline void string(vec2 const& position, attribute attrib,
    bool const centered, wchar_t const* const str) {
  assert(position.x >= 0 && position.y >= 0);
  assert(position.y < state.size.y);

  // number of characters in the string (but not necessarily the number of 
  // characters that will be drawn)
  auto const size = lstrlenW(str);
  if (size <= 0)
    return;

  // first character index
  auto start = (size_t)position.x + (size_t)position.y * state.size.x;

  // apply centering if requested
  if (centered) {
    auto const len = string_length(str);
    if (len / 2 > position.x)
      start -= position.x;
    else
      start -= len / 2;
  }

  size_t xpos = 0;

  // render each character in the string
  for (size_t i = 0; i < size; ++i) {
    // we reached the end
    if (position.x + xpos >= state.size.x)
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

    state.backbuffer[start + xpos] = {
      str[i], *(uint16_t*)&attrib
    };

    xpos += 1;
  }
}

} // namespace impl

// setup the console
inline void initialize(vec2 const& size) {
  impl::state.handle = GetStdHandle(STD_OUTPUT_HANDLE);

  w32c::size(size);

  auto const window = GetConsoleWindow();

  // prevent resizing the console window
  SetWindowLong(window, GWL_STYLE, GetWindowLong(
    window, GWL_STYLE) & ~(WS_MAXIMIZEBOX | WS_SIZEBOX));
}

// write the backbuffer to the console window
inline void flush() {
  SMALL_RECT region{
    0, 0, (short)impl::state.size.x, (short)impl::state.size.y
  };

  // write to console
  WriteConsoleOutput(
    impl::state.handle,
    impl::state.backbuffer.get(),
    { region.Right, region.Bottom },
    { 0, 0 }, &region);
}

// resize the console window and clear the backbuffer
inline void size(vec2 const& size) {
  impl::state.size = size;

  auto const num_chars = (size_t)size.x * (size_t)size.y;
  assert(num_chars > 0);

  // allocate the backbuffer
  impl::state.backbuffer = std::make_unique<CHAR_INFO[]>(num_chars);

  // zero memory
  memset(impl::state.backbuffer.get(), 0, num_chars * sizeof(CHAR_INFO));

  SMALL_RECT const rect{
    0, 0, (short)size.x - 1, (short)size.y - 1
  };

  // resize the actual console window
  SetConsoleScreenBufferSize(impl::state.handle, { (short)size.x, (short)size.y });
  SetConsoleWindowInfo(impl::state.handle, TRUE, &rect);
}

// get the size of the console (measured in characters)
inline vec2 size() {
  return impl::state.size;
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

// hide the blinking cursor
inline void hide_cursor() {
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(impl::state.handle, &info);

  info.bVisible = false;
  SetConsoleCursorInfo(impl::state.handle, &info);
}

// unhide (show) the blinking cursor
inline void show_cursor() {
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(impl::state.handle, &info);

  info.bVisible = true;
  SetConsoleCursorInfo(impl::state.handle, &info);
}

// move the cursor to a specific position
inline void move_cursor(vec2 const& position) {
  SetConsoleCursorPosition(impl::state.handle,
    { (short)position.x, (short)position.y });
}

// is the cursor visible?
inline bool cursor_visible() {
  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(impl::state.handle, &info);
  return info.bVisible;
}

// set the input color (when someone types in console)
inline void input_color(attribute const attrib) {
  SetConsoleTextAttribute(impl::state.handle, *(uint16_t*)&attrib);
}

// shorthand for fill({ black, black }, L' ');
inline void clear() {
  fill({ black, black }, L' ');
}

// fill the console with a single character
inline void fill(attribute const attrib, wchar_t const c) {
  // loop through every character and assign it our attribute and char
  for (size_t i = 0; i <
      (size_t)impl::state.size.x * (size_t)impl::state.size.y; ++i) {
    impl::state.backbuffer[i] = {
      c, *(uint16_t*)&attrib
    };
  }
}

// render a single character to the console
inline void character(vec2 const& position, attribute const attrib, wchar_t const c) {
  // the index of this position in the character array
  auto const index = position.x + position.y * impl::state.size.x;

  // make sure we're in bounds
  assert(position.x >= 0 && position.x < impl::state.size.x);
  assert(position.y >= 0 && position.y < impl::state.size.y);

  impl::state.backbuffer[index] = {
    c, *(uint16_t*)&attrib
  };
}

// render a string to the console
template <typename ...Args>
inline void string(vec2 const& position, attribute const attrib, 
    wchar_t const* const format, Args&& ...args) {
  wchar_t buffer[1024];

  // format our string
  auto const swprintf_s_return_value = swprintf_s(
    buffer, format, std::forward<Args>(args)...);

  // maybe the buffer is too small
  assert(swprintf_s_return_value != -1);

  // forward to real function
  impl::string(position, attrib, false, buffer);
}

// render a horizontally centered string to the console
template <typename ...Args>
inline void stringc(vec2 const& position, attribute const attrib,
    wchar_t const* const format, Args&& ...args) {
  wchar_t buffer[1024];

  // format our string
  auto const swprintf_s_return_value = swprintf_s(
    buffer, format, std::forward<Args>(args)...);

  // maybe the buffer is too small
  assert(swprintf_s_return_value != -1);

  // forward to real function
  impl::string(position, attrib, true, buffer);
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
  auto const should_hide_cursor = !cursor_visible();
  bool success = true;

  // make sure we know where to type
  show_cursor();
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
    hide_cursor();

  return success;
}

} // namespace w32c