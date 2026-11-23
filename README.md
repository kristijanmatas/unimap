# unimap

A fast, dependency-free **Unicode character map for the terminal**, in the spirit of
GNOME Character Map. Browse every Unicode block, search characters by name, collect
them into a text buffer and copy the result to the clipboard without leaving the
terminal.

Written in plain C using only libc (termios + ANSI escape sequences, **no ncurses**).

<img width="967" height="536" alt="unimap screenshot" src="https://github.com/user-attachments/assets/f5d0b18d-de82-4646-ac73-946a6a529fe2" />

## Features

- **Block browser:** every Unicode block in a side list, with a character grid next to it
- **Name search:** find characters by name (`arrow up`, `snowman`, `greek small alpha`)
  or by block name; searches all blocks or only the current one
- **Quick jump:** go to any code point (`g`, hex), paste a character to jump to it,
  or type `U+XXXX` in the search prompt
- **Text buffer:** collect several characters and copy them all at once
- **Clipboard:** OSC 52 (works in most modern terminals, even over SSH), plus
  `wl-copy`, `xclip`, `xsel` or `pbcopy` when available
- **Color schemes:** 8 built-in themes (`default`, `dark`, `light`, `solarized-dark`,
  `solarized-light`, `gruvbox`, `nord`, `dracula`), switchable live with `t` / `T`
- **Mouse support:** click to select, double-click to add, scroll with the wheel
- **Vim-style keys:** `hjkl` work alongside the arrow keys
- **Single binary:** the character-name table is compiled in, so no runtime data files are needed

## Installation

### Debian, Ubuntu, Kali, Mint (apt)

```sh
curl -fsSL https://kristijanmatas.github.io/unimap/key.gpg \
  | sudo gpg --dearmor -o /usr/share/keyrings/unimap.gpg

echo "deb [signed-by=/usr/share/keyrings/unimap.gpg] https://kristijanmatas.github.io/unimap stable main" \
  | sudo tee /etc/apt/sources.list.d/unimap.list

sudo apt update
sudo apt install unimap
```

Updates then arrive with `sudo apt upgrade`. The package contains a static binary,
so it works on any 64-bit (amd64) release.

### Prebuilt binary

Download `unimap-linux-x86_64` (static, runs on any 64-bit Linux) or the `.deb` from the
[releases page](https://github.com/kristijanmatas/unimap/releases).

### From source

See [Building](#building) below.

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
make                    # or: cc -O2 -Wall -o unimap unimap.c
```

`unames.h` (the character-name table) is picked up automatically when it sits next
to `unimap.c`. To build a smaller binary **without** name search:

```sh
cc -O2 -Wall -DNO_NAMES -o unimap unimap.c
```

### Installing (optional)

```sh
sudo make install       # installs to /usr/local (binary + man page)
sudo make uninstall
```

Other targets: `make static` (statically linked binary), `make deb` (Debian package
via `dpkg-buildpackage`).

## Usage

```sh
./unimap                     # start with the default look
./unimap -t gruvbox          # start with a color scheme
./unimap -l                  # list color schemes
./unimap -h                  # help
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
| `c` / `y` | Copy the buffer to the clipboard (or the current character if the buffer is empty) |
| `g` | Go to a code point (hex) |
| `/` | Search (see below) |
| `t` / `T` | Next / previous color scheme |
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

### Color schemes

| Theme | Description |
|-------|-------------|
| `default` | Uses your terminal's own colors (bold / reverse / dim only) |
| `dark` | Neutral dark gray with blue accents |
| `light` | Light background with blue accents |
| `solarized-dark` | Solarized, dark variant |
| `solarized-light` | Solarized, light variant |
| `gruvbox` | Gruvbox dark, warm retro colors |
| `nord` | Nord, cool arctic blues |
| `dracula` | Dracula, dark with pink and purple |

Choose one with `-t NAME` (or `--theme NAME`), or set it permanently in your shell
config:

```sh
export UNIMAP_THEME=nord
```

The `-t` option overrides `UNIMAP_THEME`. Inside the app, `t` / `T` cycles through
the schemes; the current one is shown in the title bar.

All schemes except `default` use 24-bit (truecolor) escape sequences, which nearly every
modern terminal supports (GNOME Terminal, Konsole, kitty, Alacritty, WezTerm, iTerm2,
Windows Terminal, tmux with `Tc` enabled). On a terminal without truecolor support,
use `default`.

New schemes are easy to add: append an entry to the `themes[]` table in `unimap.c`.

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

## Project structure

| File | Description |
|------|-------------|
| `unimap.c` | The whole application |
| `unames.h` | Generated Unicode character-name table (do not edit by hand) |
| `gen_names.py` | Script that generates `unames.h` |
| `unimap.1` | Man page |
| `Makefile` | Build, install and packaging targets |
| `debian/` | Debian packaging (`dpkg-buildpackage`) |
| `packaging/` | Scripts for the static `.deb` and the apt repository |
| `.github/workflows/release.yml` | Builds and publishes packages on each GitHub release |

## Releasing

Publish a GitHub release with a tag such as `v1.1`. The *Release packages* workflow
then builds the static binary and the `.deb`, attaches them to the release and adds
the package to the apt repository on GitHub Pages. Update `debian/changelog` first
(`dch -v 1.1 "..."`).

## Contributing

Issues and pull requests are welcome. Please keep the project dependency-free
(libc only) and make sure it builds cleanly with `cc -O2 -Wall -Wextra`.

## License

[MIT](LICENSE) © 2026 Kristijan Matas

## Author

Kristijan Matas · [@kristijanmatas](https://github.com/kristijanmatas)
