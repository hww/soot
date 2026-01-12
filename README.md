# SOOT: High-Performance Lisp Engine & VM


![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=flat&logo=cmake&logoColor=white)
![Virtual Machine](https://img.shields.io/badge/Virtual_Machine-FF6B6B?style=flat&logo=virtualbox&logoColor=white)
![Compiler](https://img.shields.io/badge/Compiler-4ECDC4?style=flat&logo=gnu&logoColor=white)
![Cross Platform](https://img.shields.io/badge/Cross_Platform-8E44AD?style=flat&logo=windows&logoColor=white)
![Retro Computing](https://img.shields.io/badge/Retro_Computing-FF9A00?style=flat&logo=retroarch&logoColor=white)
![Status](https://img.shields.io/badge/Status-Active-success)


**SOOT** is an embeddable Lisp interpreter and Virtual Machine inspired by classic systems (like the Aleste LX) but engineered for modern C++ performance. It bridges the gap between a lightweight execution core and a professional development environment.

---
![REPL Screenshot](/docs/screens/repl.png)

---

## 🛠 Toolchain Architecture

The project is split into two distinct binaries to separate execution from interaction:

* **`sooti` (SOOT Interpreter Core)**: A minimalist, headless engine. Optimized for embedding into applications, running background tasks, and executing scripts with zero terminal overhead.
* **`soot` (Professional REPL)**: A high-end interactive shell powered by `replxx`. Features syntax highlighting, multiline editing, command history, and a built-in **nREPL server** for remote connectivity.

---

## 🚀 Quick Start (Linux)

### Build and Install

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

```

### Path Lookup Logic (XDG Standards)

SOOT follows standard Linux directory conventions for seamless integration:

1. **Project Layer**: Current working directory (e.g., `./config.gs`).
2. **User Layer**: `~/.config/soot/` (Personal global settings).
3. **System Layer**: `/usr/local/share/soot/` (Standard library and `lib.gs`).
4. **Persistent History**: Stored automatically in `~/.cache/soot/history`.

---

## ⚙️ Configuration (`config.gs`)

SOOT is configured using its own Lisp dialect. You can customize your environment in `~/.config/soot/config.gs`:

```lisp
'(
  (nrepl-port 8181)
  (prompt "soot> ")
  (history-size 1000)
  (keybind ctrl "L" "Clear screen" "(clear)")
  (keybind ctrl "K" "Show keybinds" "(keybinds)")
)

```

---

## 📊 Roadmap & Current Status

### Core Engine (Carbon) ✅

* [x] **Virtual Machine**: High-speed bytecode interpreter and robust type system.
* [x] **Module System**: Dynamic dependency resolution and cross-module linking.
* [x] **FFI**: Native function support for seamless C++ method invocation.
* [x] **Scheduler**: Process state management and basic task scheduling.

### Kernel & Orchestration 🔴 (In Progress)

* [ ] **IPC**: Inter-process communication via Message Passing.
* [ ] **Event System**: Event-driven architecture between isolated processes.
* [ ] **Error Recovery**: Automatic supervisor-style recovery for failed processes.

### Advanced Features 🟡 (Planned)

* [ ] **State Machines**: Hierarchical State Machine (HSM) definitions.
* [ ] **Async/Coroutines**: Native support for async frames and generators.
* [ ] **JIT Compilation**: Experimental JIT for performance-critical hotpaths.

---

## 🤝 Development & Contribution

SOOT is designed for use in game engines and automation systems where Lisp's flexibility meets C++ speed.

**Core Dependencies:**

* `fmt`: Modern output formatting.
* `replxx`: Professional CLI input handling.
* `cross_sockets`: Network REPL functionality.

---

