
# SOOT: Scriptable Object-Oriented Toolkit

![C++](https://img.shields.io/badge/C++-17-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C?style=flat&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-8E44AD?style=flat)
![License](https://img.shields.io/badge/License-MIT-blue?style=flat)
![Build](https://github.com/your-username/soot/actions/workflows/build.yml/badge.svg)
![Tests](https://img.shields.io/badge/tests-passing-brightgreen)

**SOOT** is an embeddable Lisp interpreter and Virtual Machine designed for automation toolkits on constrained hardware (CPUs, MCUs, DSPs). Engineered with modern C++ for performance, it bridges the gap between lightweight execution and professional development.

![REPL Screenshot](/docs/screens/repl.png)

---

## ✨ Key Features

| Feature | Description |
|---------|-------------|
| **Dual Binary Architecture** | Separate interpreter core (`sooti`) and interactive REPL (`soot`) |
| **Carbon VM** | High-performance bytecode execution engine |
| **Multi-Format Support** | `.sot` (source) and `.soc` (compiled bytecode) files |
| **Embeddable** | Minimal footprint for integration into other applications |
| **Remote Development** | Built-in nREPL server for remote connectivity |
| **XDG Compliance** | Standard Linux directory structure support |
| **Modern Tooling** | Syntax highlighting, multiline editing, command history |

---

## 🚀 Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake git

# macOS
brew install cmake gcc

# Windows (MinGW/MSYS2)
pacman -S --needed base-devel mingw-w64-x86_64-toolchain cmake git
```

### Build & Install
```bash
git clone https://github.com/your-username/soot.git
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

### Docker (Alternative)
```bash
docker build -t soot .
docker run -it soot
```

---

## 📁 Project Structure

```
soot/
├── src/
│   ├── core/          # Carbon VM core
│   ├── compiler/      # SOT to SOC compiler
│   ├── interpreter/   # sooti headless interpreter
│   └── repl/         # soot interactive shell
├── include/          # Public headers
├── common/script/    # Standard library
├── examples/         # Example scripts
└── tests/           # Test suite
```

---

## 📚 Usage Examples

### Interactive REPL
```bash
# Start interactive session
soot

# Execute a script
soot script.sot

# Compile to bytecode
soot --compile script.sot -o script.soc

# Execute bytecode
soot script.soc
```

### Embedding in C++ Application
```cpp
#include <sooti/sooti.h>

int main() {
    sooti::VM vm;
    vm.load_script("config.sot");
    auto result = vm.eval("(+ 1 2 3)");
    std::cout << result << std::endl;
    return 0;
}
```

### Remote Development (nREPL)
```bash
# Start nREPL server (port 8181)
soot --nrepl

# Connect from editor (Emacs/CIDER, VSCode/Calva)
# Connect to localhost:8181
```

---

## ⚙️ Configuration

### User Configuration (`~/.config/soot/config.sot`)
```lisp
;; Global configuration
(define *nrepl-port* 8181)
(define *prompt* "soot> ")
(define *history-size* 1000)

;; Keybindings
(keybind ctrl "L" "Clear screen" "(clear-screen)")
(keybind ctrl "K" "Show keybinds" "(show-keybinds)")

;; Autoload modules
(autoload 'math 'strings 'json)

;; Environment variables
(setenv "SOOT_PATH" "/usr/local/share/soot")
```

### System Configuration (`/usr/local/share/soot/lib.sot`)
```lisp
;; Standard library definitions
(provide 'soot-core)

;; Platform-specific extensions
(cond
  ((string=? (platform) "linux") (load "linux-ext.sot"))
  ((string=? (platform) "windows") (load "win-ext.sot"))
  ((string=? (platform) "darwin") (load "macos-ext.sot")))
```

---

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md).

### Development Workflow
```bash
# 1. Fork and clone
git clone https://github.com/your-username/soot.git

# 2. Create feature branch
git checkout -b feature/amazing-feature

# 3. Make changes and test
make test

# 4. Commit and push
git commit -m "Add amazing feature"
git push origin feature/amazing-feature

# 5. Create Pull Request
```

### Code Style
- Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- Use `clang-format` for formatting
- Document public APIs with Doxygen-style comments

---

## 📖 Documentation

- 📖 [SOOT Script Language - Quick Reference](common/script/README.md)
- 📖 [SOOT Common Library Documentation](common/script/README.LIB.md)

---

## 🐛 Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| **"Command not found: soot"** | Ensure `/usr/local/bin` is in PATH |
| **Missing dependencies** | Install `libfmt-dev`, `libreplxx-dev` |
| **Permission denied** | Use `sudo make install` or set custom prefix |
| **nREPL connection failed** | Check firewall: `sudo ufw allow 8181` |

### Debug Mode
```bash
# Enable verbose output
soot --verbose script.sot

# Debug bytecode
soot --debug --compile script.sot

# Profile execution
soot --profile script.sot
```

---

## 📄 License

SOOT is released under the **MIT License**. See [LICENSE](LICENSE) for details.

```
MIT License

Copyright (c) 2024 Your Name

Permission is hereby granted...
```

---

## 🙏 Acknowledgements

- **OpenGOAL** for inspiration in Lisp implementation
- **replxx** for excellent terminal handling
- **fmt** for modern formatting library
- All contributors and users of SOOT

---

## 📞 Support & Community

- **Issues**: [GitHub Issues](https://github.com/your-username/soot/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-username/soot/discussions)
- **Email**: your-email@example.com
- **Twitter**: [@soot_lang](https://twitter.com/soot_lang)

---

**Star this repo if you find SOOT useful!** ⭐

