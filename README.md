# snake

TUI snake game using Curses and C. This project is a small exercise for myself
in C programming fundamentals, systems programming, ncurses, and documentation.
I make use of C fundamentals such as safe memory management, passing pointer
to manage game state, and use of variadic functions. I implement IPC
logging using Unix sockets for debugging purposes, as well as signal handling
for safe exit on interrupt. As an introduction to TUI's, ncurses was used.
Throughout the program I practice documentation using kernel doc format on
functions and structs.

This still requires a score counter, support for UTF-8, and safe window resizing.

## How to play

The snake (represented as #'s) must eat as many apples (represented as a's) as
possible without running into itself. This iteration of the game also supports
wrapping around the screen, so the only way to die is by running into your own tail.

Choose from 4 different speed options:

```bash
./bin/snake -s: slow
./bin/snake -f: fast
./bin/snake -S: super slow
./bin/snake -F: super fast
```

## Building

To run the game alone run:

```bash
make
./bin/snake
```

For the logger running alongside the game, run:

In one terminal:

```bash
make logger
./bin/logger
```

In a second terminal:

```bash
make
./bin/snake
```
