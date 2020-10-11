# winterm

![made with c++17](https://img.shields.io/static/v1?label=made+with&message=c%2B%2B17&color=blue&logo=c%2B%2B&logoColor=blue&style=for-the-badge)
![mit license](https://img.shields.io/static/v1?label=license&message=MIT&color=blue&style=for-the-badge)

A ***header-only*** library for the windows terminal.

## Example
```cpp
// initialize the library
term::initialize();

// set the title
term::title(L"monkey nuts");

// resize the terminal
term::size({ 140, 40 });

// disable the cursor blinking
term::disable(term::cursor);

// prevent highlighting text in the console
term::disable(term::highlighting);

std::wstring name = L"";

// clear the console
term::clear();

// draw red text in the top left
term::string({ 0, 0 }, term::red, L"hello world!");

// same thing but a blue background
term::string({ 0, 1 }, { term::red, term::blue }, L"hello world!");

// you can also add term::intense to any color
term::string({ 0, 2 }, { term::red, term::blue | term::intense }, L"hello world!");

// draw a centered string in the middle of the screen
term::stringc({ 70, 20 }, term::white, L"what is your name?");

while (name.empty()) {
  // draw to the screen
  term::flush();

  // ask for their name
  term::input({ 0, 39 }, name);
}

// clear the center of the screen by drawing an empty horizontal line
term::hline(20, 0, ' ');

// you can change colors in the middle of the stringby using #
term::stringc({ 70, 20 }, term::white, L"your name is #6X%s#7X!", name.c_str());

// draw to the screen
term::flush();

std::getchar();
```
