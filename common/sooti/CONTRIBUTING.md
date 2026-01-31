Это отличный фундамент, но для полноценного **CONTRIBUTING.md** (особенно в системном проекте вроде твоего) стоит добавить еще три критических раздела: **Architecture Principles** (почему мы пишем так), **Memory Management** (как не сломать GC) и **Error Handling**.

Вот расширенная версия, которую можно смело класть в корень репозитория:

---

# CONTRIBUTING.md

## 📜 1. Naming Conventions (The Soot Style)

*To maintain the spirit of a Compiler-Construction Lisp, follow these rules:*

* **Style:** Always use `kebab-case` (e.g., `make-lextoken`). No `camelCase` or `snake_case`.
* **Predicates:** Use `?` for general queries (`null?`) and `-p` for type predicates (`pair-p`). Avoid `is-`.
* **Mutation:** Use `!` for functions with side effects (`set-car!`, `vector-set!`).
* **Conversion:** Use `->` for transformations (`number->string`). Avoid `to`.
* **Constructors:** Use `make-` prefix (`make-hash-table`).
* **HeapObjects:** Getters use `object-field` (`lextoken-type`). Setters use `set-object-field!`.

---

## 🏗️ 2. Architectural Principles

* **Lisp-1 Semantics:** We follow Scheme-like single namespace for functions and variables.
* **Small Core, Rich Library:** Keep the C++ `Interpreter` core minimal. If a feature can be implemented in Lisp as a macro or function, it should be.
* **System Oriented:** Prioritize performance and memory transparency. Our Lisp is a tool for building assemblers and compilers, not just a general-purpose language.

---

## 🧠 3. Memory & Objects

* **Immutable vs Mutable:** Favor immutability for core types. If you introduce a new mutable type, ensure it has a `!` setter.
* **Garbage Collection:** All new `ObjectType` entries must be correctly handled in the `Interpreter`'s tracing logic (if applicable) to avoid memory leaks.
* **Value Types:** Use `Object` by value where possible; use `heap_obj` pointers only for complex structures (Lists, Hash-tables, Tokens).

---

## ⚠️ 4. Error Handling

* **Context is King:** Always use `throw_eval_error(form, message)`. The `form` argument is mandatory as it provides the user with a "pointer" (arrow) to the failing code.
* **LexTokens:** When writing compiler-related logic, propagate `LexToken` info as far as possible to ensure meaningful backtraces.

---

## 🛠️ 5. Development Workflow

1. **C++ Changes:** If you add a `builtin`, register it in `init_builtin_forms` and provide a docstring if the system supports it.
2. **Lisp Changes:** Place core Lisp logic in `core.soot` or equivalent initialization files.
3. **Testing:** Every new feature should be testable via the REPL.

---

## 🛠️ 6. Macro Philosophy

Macros are a powerful tool for syntax transformation, but they should be used judiciously to keep the language predictable and maintainable.

### The Golden Rule of Macros

> **"If it can be a function, it should be a function."**

Macros should only be used when functions are insufficient. Specifically, use macros for:

1. **Syntax Extensions:** Creating new language constructs like `while`, `cond`, or `let`.
2. **Short-circuiting/Control Flow:** When you need to control if or when an expression is evaluated (like `and`, `or`, or `if`).
3. **Code Generation:** Automating repetitive boilerplate for the assembler (e.g., a macro to define multiple opcode variations).
4. **Binding:** When you need to introduce new variables into the scope.

### Macro Best Practices

* **Avoid Double Evaluation:** Ensure that macro arguments are evaluated only once in the expanded code to prevent side-effect bugs.
* **Variable Capture (Hygiene):** Use `gensym` to create unique symbols inside your macros to avoid accidental name collisions with user variables.
* **Readability over Magic:** A macro should expand into clear, idiomatic Lisp. If the expansion is too complex to debug, simplify the logic or move it to a helper function.
* **Compile-Time vs Run-Time:** Remember that macros perform transformations at *read/compile time*. They do not have access to the *values* of variables at runtime, only to their *shapes* (the code itself).
