<img width="967" height="536" alt="Screenshot From 2026-11-23 15-40-52" src="https://github.com/user-attachments/assets/f5d0b18d-de82-4646-ac73-946a6a529fe2" />

# unimap

A fast, dependency-free **Unicode character map for the terminal**, in the spirit of
GNOME Character Map. Browse every Unicode block, search characters by name, collect
them into a text buffer and copy the result to the clipboard without leaving the
terminal.

Written in plain C using only libc (termios + ANSI escape sequences, **no ncurses**).

## Features

- **Block browser:** every Unicode block in a side list, with a character grid next to it
- **Name search:** find characters by name (`arrow up`, `snowman`, `greek small alpha`)
  or by block name; searches all blocks or only the current one
- **Quick jump:** go to any code point (`g`, hex), paste a character to jump to it,
  or type `U+XXXX` in the search prompt
- **Text buffer:** collect several characters and copy them all at once
- **Clipboard:** OSC 52 (works in most modern terminals, even over SSH), plus
  `wl-copy`, `xclip`, `xsel` or `pbcopy` when available
- **Mouse support:** click to select, double-click to add, scroll with the wheel
- **Vim-style keys:** `hjkl` work alongside the arrow keys
- **Single binary:** the character-name table is compiled in, so no runtime data files are needed

## Requirements

- A C compiler (`gcc` or `clang`) on Linux, macOS or another POSIX system
- A **UTF-8** terminal, plus a font that has the glyphs you want to see
  (e.g. Noto, DejaVu, or a Nerd Font)
- Optional: `wl-copy` (Wayland), `xclip` / `xsel` (X11) or `pbcopy` (macOS) for
  clipboard access when your terminal does not support OSC 52
- Optional: Python 3, only if you want to regenerate the character-name table

## Building

```sh
git clone https://github.com/kristijanmatas/unimap.git
cd unimap
cc -O2 -Wall -o unimap unimap.c
```

`unames.h` (the character-name table) is picked up automatically when it sits next
to `unimap.c`. To build a smaller binary **without** name search:

```sh
cc -O2 -Wall -DNO_NAMES -o unimap unimap.c
```

### Installing (optional)

```sh
sudo install -m 755 unimap /usr/local/bin/
```

## Usage

```sh
./unimap
```

### Key bindings

| Key | Action |
|-----|--------|
| `Tab` / `Shift-Tab` | Switch between block list and character grid |
| Arrows / `h` `j` `k` `l` | Move |
| `PgUp` / `PgDn` | Page up / down |
| `Home` / `End` | Start / end of row (grid), first / last block (list) |
| `n` / `p` or `]` / `[` | Next / previous block |
| `Enter` / `Space` | Add character to the text buffer (`Enter` in the block list jumps to the grid) |
| `Backspace` | Delete last character from the buffer |
| `x` | Clear the text buffer |
| `c` | Copy the buffer to the clipboard |
| `y` | Copy the current character to the clipboard |
| `g` | Go to a code point (hex) |
| `/` | Search (see below) |
| Mouse | Click to select, double-click to add, wheel to scroll |
| `q` / `Ctrl-C` | Quit |

### Search

Press `/` and type:

- part of a **character name**, e.g. `arrow up`, `snowman`, `check mark`
- a **block name**, e.g. `emoticons`, `box drawing`
- a **single character** (typed or pasted) to jump straight to it
- a code point like `U+2603`

`Tab` inside the prompt toggles the scope between *all blocks* and *current block*.
In the results list: `Up`/`Down` to move, `Enter` to go to the character, `Space` to
add it to the text buffer, `Esc` to close.

## Regenerating the name table

`unames.h` is generated from Python's `unicodedata` module. A newer Python ships a
newer Unicode version and therefore more character names:

```sh
python3 gen_names.py > unames.h
cc -O2 -Wall -o unimap unimap.c
```

Algorithmic names (CJK ideographs, Tangut, etc.) are stored as compact ranges, and
Hangul syllable names are computed at runtime, which keeps the table small.
The currently committed table covers **Unicode 15.0**.
<img width="967" height="536" alt="Screenshot From 2026-11-23 15-40-52" src="https://github.com/user-attachments/assets/f5d0b18d-de82-4646-ac73-946a6a529fe2" />

## Project structure

| File | Description |
|------|-------------|
| `unimap.c` | The whole application |
| `unames.h` | Generated Unicode character-name table (do not edit by hand) |
| `gen_names.py` | Script that generates `unames.h` |

## Contributing

Issues and pull requests are welcome. Please keep the project dependency-free
(libc only) and make sure it builds cleanly with `cc -O2 -Wall -Wextra`.

## Author

Kristijan Matas · [@kristijanmatas](https://github.com/kristijanmatas)
