# Game Scripting System 

## State

Before Alfa

## Brief description of the project

The project is based on information found on the Internet about the scripting language and runtime system used by Naughty Dog.

The project consists of two parts: the source code of the virtual machine in C++ and the source code of the compiler for this machine written in Racket.

**Disclaimer**

I don’t work at Naughty Dog, nor do I have any secret knowledge of The Last of Us,
except what I figured out myself from the disc. So a lot of this may well be wrong. Take
it with a pinch of salt. Most of the code samples in this document are taken from the
sources listed at the end of the document.

## List of main directories

The following files and folders are located in the main project directory.

- vm - Virtual machine source code
- vmc - Source code of the compiler
- vmc-test - Tests for the compiler

Each directory can have its own .md file

## Task list

The project has been falling apart quickly and therefore some decisions may require revisions and modifications. But since they are not a high priority and may not need to be changed at all, I am including them in the list of optional tasks.

It is important to understand that having a built-in assembler is only a tool to help with development. Eventually it will not be used and instead a code compiler will be created for the virtual machine based on the Racket programming language.

### Main tasks

- [x] The system of generation of name and type identifiers based on the crc32 algorithm
- [x] The source code of the virtual machine and the data stack for the processes. A set of instructions as well as the container of the main data type in the form of the variant class.
- [x] Binary and text module files as well as module import and export downloads.
- [x] Tokenizer and parser of text s-expression files.
- [x] The module header (export, import) file parser
- [x] Bytecode Assembler and Disassembler
- [ ] Top level functionality
  - [ ] Process Management
  - [ ] Exception handling
  - [ ] Logging
  - [ ] REPL
- [ ] DSL compiler to virtual machine code. 
- [ ] Virtual machine code coverage -- automatic tests
- [ ] Performance optimization: custom allocators, object pools -- StackFrame, Vector3, Quaternion, ...
- [ ] Additional utilities for verification, incremental compilation, and generation of debugging information files
- [ ] Integration with UnrealEngine
- [ ] Integration in Unity3D

### Optional tasks

- [ ] Replacing the parser with support for the Scheme polish standard syntax
- [ ] Support for data structures in the bytecode assembler and disassembler

## References

- Ming-Lun “Allen” Chou. Melee ai in "the last of us part ii". 2021.
- Terrence Cohen. A dynamic component architecture for high performance gameplay. 2010.
- Jason Gregory. State based scripting in "uncharted: Drake’s fortune". 2006.
- Jason Gregory. Game engine architecture. 2014.
- Jason Gregory. Game object model and scripting. 2017.
- Dan Liebgold. Adventures in data compilation in "uncharted: Drake’s fortune". 2008.
