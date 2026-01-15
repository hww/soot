
# SOOT: Scriptable Object-Oriented Toolkit

![C++](https://img.shields.io/badge/C++-17-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C?style=flat&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-8E44AD?style=flat)
![License](https://img.shields.io/badge/License-MIT-blue?style=flat)
![Build](https://github.com/your-username/soot/actions/workflows/build.yml/badge.svg)
![Tests](https://img.shields.io/badge/tests-passing-brightgreen)

## SOOT & Carbon: A Tiered Ecosystem for Embedded Automation

**SOOT** is an embeddable Lisp interpreter, inherited from **GOOS** and engineered with modern C++, designed to orchestrate automation toolkits for constrained hardware (CPUs, MCUs, DSPs). It serves as the high-level control center, bridging the gap between raw source code and final deployment.

### The Architecture: SOOT and Carbon

The system is divided into two distinct layers to separate high-level logic from low-level execution:

* **SOOT (The Orchestrator):** A flexible Lisp interpreter used to design scripts for compiling and linking entire projects. Beyond simple builds, SOOT acts as a **preprocessor**, capable of modifying source files on the fly. Its integrated **REPL** allows developers to inspect and debug every stage of the pipeline—from code transformation to target communication—in an iterative loop.
* **Carbon (The Execution Layer):** A specialized environment consisting of its own **Virtual Machine** and **Compiler**. While Carbon acts as the foundation for execution, the SOOT interpreter remains independent, acting as the manager that directs the Carbon compiler's operations.

> **Note:** The Carbon Virtual Machine is fully functional, while the Carbon Compiler is currently under active development.

![REPL Screenshot](/docs/screens/repl.png)

### "Why SOOT?" Section

Since this is for constrained hardware, adding a small "Design Goals" list can help users understand your philosophy:

    Zero-Dependency Core: Minimal external requirements.

    Memory Efficiency: Tailored for MCUs and DSPs.

    Developer-Centric: Built-in REPL for rapid hardware prototyping.

### Carbon Status

You’ve noted that the Carbon Compiler is in development. You might want to add a tiny "Roadmap" bullet point at the bottom to show what's coming next (e.g., "JIT support for Carbon VM" or "C++ FFI").

### Integrated Workflow

SOOT provides a unified interface for the entire development lifecycle:

1. **Preprocessing:** Dynamic modification of source code via Lisp scripts.
2. **Orchestration:** Managing the compilation and linking process for the target platform.
3. **Automation:** Handling firmware upload and establishing a communication link with the hardware.
4. **Inspection:** Using the REPL at any stage for real-time debugging and system analysis.

---

## ✨ Key Features

| Feature                | Description                                                                          |
|------------------------|--------------------------------------------------------------------------------------|
| **Orchestration Core** | Lisp interpreter (`sooti`) for build automation, linking, and source preprocessing.  |
| **Carbon VM**          | A high-performance bytecode execution engine for target platforms, managed by SOOT.  |
| **Interactive REPL**   | Full-featured environment (`soot`) for real-time debugging of the entire pipeline.   |
| **Hybrid Execution**   | Supports `.sot` (Lisp source) and the upcoming `.soc` (Carbon compiled bytecode).    |
| **Embedded & Remote**  | Minimal footprint for integration with an nREPL server for remote hardware links.    |
| **Modern Tooling**     | XDG compliance, syntax highlighting, multiline editing, and command history.         |
| **Carbon Compiler**    | **(In Development)** Native toolchain to bridge SOOT logic with Carbon VM execution. |

---

## 🚀 Quick Start

### Build & Install

```bash
git clone https://github.com/hww/soot.git
cd soot

# Configure
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
make -j$(nproc)

# Install
sudo make install

# Verify installation
soot --version
```

---

## 📚 Usage Guide

SOOT provides a unified interface for scripting, project management, and interactive debugging.

### Command Line Interface

| Flag               | Short | Description                                        |
|--------------------|-------|----------------------------------------------------|
| `--script <file>`  | `-s`  | Executes a specific Lisp script and exits.         |
| `--network [port]` | `-n`  | Starts a Network REPL server (default: 8181).      |
| `--project <path>` | `-p`  | Sets the working project path for file operations. |
| `--help`           | `-h`  | Displays the help message.                         |

### Execution Modes

#### 1. Interactive REPL

Start a local interactive session for real-time experimentation:

```bash
bash> soot
```

* 📖 [SOOT Script Language - Quick Reference](common/sooti/README.md)
  
#### 2. Network REPL (nREPL)

Launch a network server to connect from external editors (like Emacs/CIDER or VSCode/Calva). Note that SOOT starts both the network server and a local interactive session simultaneously:

```bash
# Start server on default port 8181
bash> soot --network

# Start server on a custom port
bash> soot --network 9090
```

#### 3. Script Execution & Project Management

Use SOOT as an orchestrator for your build process by specifying a project directory and a script:

```bash
# Run a specific automation script
bash> soot --script build.lisp

# Run with a defined project context
bash> soot --project ./my_mcu_project --script deploy.lisp
```

### Technical Note: Project Context

The `--project` flag is a key feature for your automation workflow. It sets the base path for `file_util`, allowing your Lisp scripts to use relative paths when modifying source files or linking binaries, ensuring the automation logic remains portable across different environments.

---

## ⚙️ Configuration

User Configuration (`~/.config/soot/config.sot`)

```lisp
;; -- Global configuration ----------------------
(define *nrepl-port* 8181)
(define *prompt* "soot> ")
(define *history-size* 1000)

;; -- Keybindings -------------------------------
(keybind ctrl "L" "Clear screen" "(clear-screen)")
(keybind ctrl "K" "Show keybinds" "(show-keybinds)")

;; -- Autoload modules --------------------------
(autoload 'math 'strings 'json)

;; -- Environment variables ---------------------
(setenv "SOOT_PATH" "/usr/local/share/soot")
```

## 📦 Core Library & Platform Adaptation

SOOT includes a minimal Standard Library located at /usr/local/share/soot/lib.sot. This library is automatically loaded to provide essential definitions and to handle cross-platform hardware abstractions.

* 📖 [SOOT Common Library Documentation](common/sooti/README.LIB.md)

## 📁 Configuration Paths and Files

SOOT follows modern standards (such as XDG) to ensure portability and clean organization across different operating systems.

### Path Resolution Table

| Path Type   | Description               | Resolution Logic                              |
|-------------|---------------------------|-----------------------------------------------|
| **CWD**     | Current Working Directory | `fs::current_path()`                          |
| **EXE**     | Executable Directory      | System-specific binary location               |
| **HOME**    | User Home Directory       | `$HOME` or current path fallback              |
| **CONFIG**  | Configuration Files       | `$XDG_CONFIG_HOME/soot/` or `~/.config/soot/` |
| **CACHE**   | Cache and History         | `$XDG_CACHE_HOME/soot/` or `~/.cache/soot/`   |
| **SHARE**   | System-wide Assets        | `/usr/local/share/soot/`                      |
| **PROJECT** | Project Root              | Set via `--project` flag or auto-detected     |

### Search Priority

The `find_config_file()` function resolves file locations using the following priority:

1. **Project Level:** `PROJECT_PATH/filename` (Highest priority)
2. **User Level:** `CONFIG_PATH/filename`
3. **System Level:** `SHARE_PATH/filename` (Fallback)

### Core Configuration Files

**1. Main Configuration (`config.sot`)**

Defines the global behavior of the REPL and environment:

* **nREPL Port:** Default network listening port.
* **Prompt:** Customizable REPL interface string.
* **Keybinds:** Custom keyboard shortcuts.

**1. Startup Sequence**

SOOT executes a two-stage initialization process:

* **Startup-Pre (`startup-pre.sot`):** Executed *before* network initialization. Used for environment setup and loading base libraries.
* **Startup-Post (`startup-post.sot`):** Executed only if the Network REPL starts successfully. Used for post-connection logic and extended networking settings.

**3. Standard Library (`lib.sot`)**

Loaded automatically during the boot process (after `startup-pre.sot`) to provide the standard Lisp functional core.

### Syntax and Customization

#### Configuration Format

Files use a native Lisp-like syntax for readability and consistency:

```lisp
(nrepl-port 7888)
(prompt "soot> ")
(keybind ctrl "K" "Clear screen" "(clear)")
```

#### Keybindings

The system supports three primary modifiers for interactive productivity:

* `ctrl` — Control + Key
* `shift` — Shift + Key
* `meta` — Meta/Alt + Key

#### Path Manipulation Functions

The interpreter exposes built-in functions to interact with the file system:

* `(get-path 'symbol)` — Retrieves the absolute path for a specific type (e.g., `'CONFIG`).
* `(find-file "filename")` — Locates a file based on the search priority rules.

---

### Environment Variables & Directories

SOOT respects the following environment variables:

* `HOME`: Base user directory.
* `XDG_CONFIG_HOME`: Standard location for configuration files.
* `XDG_CACHE_HOME`: Standard location for volatile data (e.g., command history).

**Note:** The **Cache** folder is created automatically when the history is loaded. Other directories are verified during search but are not created by the system to maintain a minimal footprint.

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md).

## 🌿 Development Workflow

```bash
# 1. Fork and clone
git clone https://github.com/hww/soot.git

# 2. Create feature branch
git checkout -b feature/amazing-feature

# 3. Make changes and test
make test

# 4. Commit and push
git commit -m "Add amazing feature"
git push origin feature/amazing-feature

# 5. Create Pull Request
```

## 🎨 Emacs Configuration

The following EMacs config file should get you started and configure OpenGOAL's formatting style

```lisp
;; make .sot files use lisp-mode
(add-to-list 'auto-mode-alist '("\\.sot\\'" . lisp-mode))
;; run setup-soot when we enter lisp mode
(add-hook 'lisp-mode-hook 'setup-soot)

(defun setup-soot ()
  ;; if we are in a gc file, change indent settings for SOOL
  (when (and (stringp buffer-file-name)
             (string-match "\\.gc\\'" buffer-file-name))
    (put 'with-pp      'common-lisp-indent-function 0)
    (put 'while        'common-lisp-indent-function 1)
    (put 'rlet         'common-lisp-indent-function 1)
    (put 'until        'common-lisp-indent-function 1)
    (put 'countdown    'common-lisp-indent-function 1)
    (put 'defun-debug  'common-lisp-indent-function 2)
    (put 'defenum      'common-lisp-indent-function 2)

    ;; indent for common lisp, this makes if's look nicer
    (custom-set-variables '(lisp-indent-function 'common-lisp-indent-function))
    (autoload 'common-lisp-indent-function "cl-indent" "Common Lisp indent.")
    ;; use spaces, not tabs
    (setq-default indent-tabs-mode nil)
    )
  )
```

## 🎯 Code Style

* Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
* Use `clang-format` for formatting
* Document public APIs with Doxygen-style comments

---

## 📖 Other Documentation

* [SOOT Script Language - Quick Reference](common/sooti/README.md)
* [SOOT Common Library Documentation](common/sooti/README.LIB.md)

---

## 📄 License

SOOT is released under the **MIT License**. See [LICENSE](LICENSE) for details.

---

⭐ **Star this repo if you find SOOT useful!** ⭐
