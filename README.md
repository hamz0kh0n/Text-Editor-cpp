# Text Editor in C++

A console-based text editor built from scratch in C++. Every line of text is a node in a **doubly linked list**, and undo/redo are powered by two **linked-list stacks**. No STL containers are used for the core data structures.

## Features

- Insert, edit, delete and move lines
- Copy, cut and paste lines
- Undo and redo, with grouped actions (Replace All reverses in a single undo)
- Recursive keyword search (case-sensitive or not)
- Find & Replace
- Word, line and character statistics
- Save and load text files, with unsaved-changes warnings
- Input validation: invalid input never crashes the program

## Data structures used

| Concept | Where it is used |
|---|---|
| Doubly linked list | Stores the document, one node per line |
| Stack (linked list) | Undo stack and redo stack |
| Recursion | Keyword search and Find & Replace |
| Dynamic memory | All nodes and commands are freed on exit |

The list keeps both a head and a tail pointer, so adding at the end is O(1), and finding a line walks from whichever end is closer.

## How to run

Requires a C++ compiler such as g++.

    g++ -std=c++11 main.cpp -o main
    ./main

On Windows, run `main.exe` instead of `./main`.

## Tests

    g++ -std=c++11 test_editor.cpp -o test_editor
    ./test_editor

65 automated checks cover insert, delete, edit, undo/redo, grouped actions, search, replace, file save/load and memory cleanup. The tests were also run with AddressSanitizer to confirm there are no memory errors or leaks.

## Project structure

- `editor.h`: data structures and all editor logic
- `main.cpp`: menu and program entry point
- `test_editor.cpp`: automated tests
- `Makefile`: build shortcuts
