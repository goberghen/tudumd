tudumd — Markdown-native fork of TuDu (0.10.5)
==============================================

`tudumd` is a Markdown-native fork of TuDu (originally written by Ruben Pollan
Bella <meskio@sindominio.net>). It preserves the complete ncurses hierarchical
TUI, keybindings, categories, priorities, deadlines, scheduled tasks, and
theming of TuDu while replacing the default XML storage backend with clean,
human-readable Markdown stored according to the XDG Base Directory
Specification.

Key additions in the `tudumd` fork (by goberghen & Arena.ai Agent, 2026):
  * Native Markdown (`.md`) parser and writer compatible with Okular,
    Ghostwriter, Quillpad, and CommonMark/GFM viewers.
  * Full XDG Base Directory Specification compliance (no dotfiles created in
    the root of `~/`):
      - Default task list:   `$XDG_DATA_HOME/tudumd/tudu.md`
                             (defaults to `~/.local/share/tudumd/tudu.md`)
      - Default user config: `$XDG_CONFIG_HOME/tudumd/tudumdrc`
                             (defaults to `~/.config/tudumd/tudumdrc`)
  * Preserves all TuDu fields in Markdown: hierarchy (4-space indentation),
    done status (`- [ ]` / `- [x]`), collapse state (`[+]`), priority
    (`!1`..`!5`), categories (`#work`, `#home`), deadlines
    (`@due(dd.mm.yyyy)`), scheduled dates (`@sched(dd.mm.yyyy)`), and
    multi-line descriptions (`> ` blockquotes with two trailing spaces).
  * Automatic read-only backward compatibility for legacy TuDu `.xml` files
    (including automatic one-time migration of `~/.tudu.md` or `~/.tudu.xml`
    into `~/.local/share/tudumd/tudu.md` on first run).
  * Fail-safe parser protection: if an existing `.md` or `.xml` file contains a
    syntax error, `tudumd` reports the line number and exits without modifying
    or overwriting your file with the welcome template.
  * Single rolling backup in `/tmp/tudu.md.bak` on every save (no `.bak` file
    clutter in working directories).
  * Includes `tuduconvert`, a standalone bidirectional CLI converter between
    `.xml` and `.md` (`tuduconvert tudu.xml newtudu.md` and
    `tuduconvert tudu.md newtudu.xml`).
  * Includes a freedesktop `.desktop` launcher (`Terminal=true`) and vector SVG
    icon (`tudumd.svg`) to launch `tudumd` directly in a terminal from the
    desktop application menu.


Install
=======

`tudumd` requires a C++ compiler and the `ncursesw` library (ncurses with wide
character support, e.g. `libncurses-dev` on Debian/Ubuntu).

To compile and install from source:
  $ ./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man
  $ make
  # make install

To build an installable Debian/Ubuntu `.deb` package:
  $ ./build-deb.sh


Usage
=====

Usage: tudumd [options]
        -f file   load tudumd markdown (or legacy xml) file
        -c file   load specific config file
        -v        show version
        -vv       show license and copyright
        -h        show usage message

Converter utility:
        tuduconvert input.xml output.md
        tuduconvert input.md  output.xml

Default files:
  * Default task file:   $XDG_DATA_HOME/tudumd/tudu.md (~/.local/share/tudumd/tudu.md)
  * User config file:    $XDG_CONFIG_HOME/tudumd/tudumdrc (~/.config/tudumd/tudumdrc)
  * Global config file:  /etc/tudumdrc (fallback: /etc/tudurc)
  * Session lock file:   ~/.local/share/tudumd/.tudu.md_lock
  * Rolling backup file: /tmp/tudu.md.bak


Notes from Original TuDu README
===============================

* Since version 0.8 the theme config `deadlineMark` is called `warn`, because it
  is used not only for deadlines but also for past scheduled tasks.

* Some terminal environments do not distinguish bold characters from normal
  characters. In such cases you can enable `(+)` for collapsed parent tasks and
  `(-)` for expanded parent tasks by setting in your config file:

  [ general ]
  visual_tree = yes
