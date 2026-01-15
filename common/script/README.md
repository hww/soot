# 📖 **SOOT Script Language - Quick Reference**

SOOT (Scriptable Object-Oriented Toolkit) - Scheme-like embedded scripting language for C++ applications.

## 📚 **Table of Contents**

- [Argument Syntax](#argument-syntax)
- [Special Forms](#special-forms)
- [Built-in Functions](#built-in-functions)
- [Mathematics](#mathematics)
- [Strings](#strings)
- [Lists and Pairs](#lists-and-pairs)
- [Vectors](#vectors)
- [Type Predicates](#type-predicates)
- [Comparisons](#comparisons)
- [Hash Tables](#hash-tables)
- [File I/O](#file-io)
- [System](#system)
- [Type Conversions](#type-conversions)
- [Time Functions](#time-functions)
- [Other Functions](#other-functions)
- [Other Documentation](#other-documentation)
---

## 📋 **Argument Syntax**

### **Basic Syntax**

```scheme
; Required arguments
(lambda (x y z) (+ x y z))          ; x, y, z are required

; Variable number of arguments
(lambda args ...)                   ; all args collected in 'args'
(lambda (a b &rest rest) ...)      ; a, b required, rest in 'rest'

; Keyword arguments (must start with :)
(lambda (&key name age) ...)       ; optional keyword args
(lambda (&key (name "John")) ...) ; keyword with default value
```

### **Calling Functions**

```scheme
(function arg1 arg2 :key1 value1 :key2 value2)
;                           ↑
; Keyword args must have colon prefix!
```

### **Examples**

```scheme
; Correct:
((lambda (x &key flag) (list x flag)) 5 :flag #t)
; → (5 #t)

; WRONG (missing colon):
((lambda (x &key flag) ...) 5 flag #t)  ; Error!

; With default values:
((lambda (&key (width 100) &key (height 200)) 
   (* width height)) :width 50)
; → 10000  (width=50, height=200 default)
```

### **Argument Order Rules**

1. Required positional arguments first
2. `&key` keyword arguments (optional)
3. `&rest` variable arguments (last)

**NOT SUPPORTED:** `&optional`, `&body` (use `&rest` instead)

---

## 🎯 **Special Forms**

Special forms don't evaluate their arguments automatically.

### **Variable Definition**

```scheme
(define x 10)                  ; Define global variable
(set! x 20)                    ; Modify existing variable
```

### **Conditionals**

```scheme
(if condition then-expr else-expr)
(cond 
  (condition1 expr1)
  (condition2 expr2)
  (else default-expr))
```

### **Logical Operators**

```scheme
(and expr1 expr2 ...)         ; Short-circuit AND
(or expr1 expr2 ...)          ; Short-circuit OR
```

### **Functions & Macros**

```scheme
(lambda (args) body...)       ; Function definition
(macro (args) body...)        ; Macro definition
(let ((var1 val1) ...) body)  ; Local bindings
(let* ((var1 val1) ...) body) ; Sequential bindings
```
Here is a polished and more professional version of the documentation for your Reader Macros. It covers the simple substitution, lambda-based processing, and the important edge cases we solved (like character literals).

---

## **Customizing the Reader (Reader Macros)**

Reader Macros allow you to extend the Soot syntax by defining how specific characters or sequences are parsed. You can use them for simple symbol substitution or complex structural transformations.

### **1. Simple Symbol Substitution**

You can map a character to a specific symbol. This is useful for creating shorthand "tags."

```scheme
;; Register '$' as a macro that expands to the symbol 'label'
soot> (set-reader-macro "$" "label")
=> #t

;; The reader now wraps the following object with the 'label' symbol
soot> (read-data "$1945")
=> (label 1945)

```

To revert the syntax to its original state, use `remove-reader-macro`:

```lisp
soot> (remove-reader-macro "$")
=> #t

soot> (read-data "$1945")
=> $1945

```

---

### **2. Functional Reader Macros (Lambda)**

For more advanced syntax, like custom brackets or data structures, you can pass a `lambda` to `set-reader-macro`. The lambda receives one argument: the **Reader** stream.

#### **Example: Implementing Square Bracket Vectors `[ ]**`

This macro captures everything between `[` and `]` and converts it into a native vector.

```lisp
;; Define '[' to read a list until it hits ']' and apply the 'vector' function
(set-reader-macro #\[ 
  (lambda (r)
    (let ((lis (read-delimited-list #\] r)))
      (apply 'vector lis))))

;; Define ']' as a terminator to prevent it from being read as a stray symbol
(set-reader-macro #\] 
  (lambda (r) 
    (error "Unexpected closing bracket")))

```

**Usage:**

```scheme
soot> [1 2 3]
=> #(1 2 3)

```

---

### **3. Character Literals and Escape Logic**

Soot’s reader is designed to respect Lisp character literal rules. Even if a character is registered as a macro (like `[`), it will be ignored when escaped as a character literal.

| Input | Interpretation | Result |
| --- | --- | --- |
| `[` | **Reader Macro** | Triggers the vector lambda |
| `#\[` | **Character Literal** | Returns the character object `[` |
| `#\newline` | **Named Character** | Returns the newline character (ASCII 10) |

This ensures that your code can still manipulate the brackets as data without accidentally triggering the macro during definition or character processing.

Implementing Hash Maps (dictionaries) using curly braces is a great exercise because, unlike vectors, it requires handling pairs of data (keys and values).

In Lisp, a common way to represent this during reading is to collect a flat list of items and then convert them into a hash map structure.
Adding Hash Map Syntax {key value ...}

To make this work, we will define a macro for { that reads everything until } and then passes that list to a hash-map constructor.

```lisp
(set-reader-macro #\{ 
  (lambda (r)
    (let ((elements (read-delimited-list #\} r)))
      (apply 'make-hash-table elements))))

(set-reader-macro #\} (lambda (r) (error "Unexpected closing brace")))
```

Usage in REPL:

```lisp
soot> { :name "Valery" :status "online" }
=> #{ :name "Valery" :status "online" }
```

### **Quoting**

```scheme
(quote expr)                  ; or 'expr
(quasiquote expr)             ; or `expr with , and ,@
```

### **Control Flow**

```scheme
(begin expr1 expr2 ...)       ; Sequence of expressions
(while condition body...)     ; While loop
```

---

## 🔧 **Built-in Functions**

## 🔢 **Mathematics**

| Function          | Arguments          | Description      |
|-------------------|--------------------|------------------|
| `+`               | `number...`        | Sum of numbers   |
| `-`               | `number number...` | Subtraction      |
| `*`               | `number...`        | Multiplication   |
| `/`               | `number number`    | Division         |
| `=`               | `number number`    | Numeric equality |
| `<` `>` `<=` `>=` | `number number`    | Comparisons      |
| `abs`             | `number`           | Absolute value   |
| `max` `min`       | `number...`        | Maximum/Minimum  |
| `expt`            | `base exponent`    | Power            |
| `sqrt`            | `number`           | Square root      |
| `ash`             | `integer shift`    | Arithmetic shift |

**Examples:**

```scheme
(+ 1 2 3)           ; → 6
(- 10 5 2)          ; → 3
(* 2 3 4)           ; → 24
(/ 10.0 2.0)        ; → 5.0
(= 5 5.0)           ; → #t
(max 1 5 3)         ; → 5
(expt 2 3)          ; → 8
(ash 4 2)           ; → 16  (4 << 2)
```

## 📝 **Strings**

| Function         | Arguments          | Description            |
|------------------|--------------------|------------------------|
| `string-append`  | `string...`        | Concatenate strings    |
| `string-length`  | `string`           | String length          |
| `string-ref`     | `string integer`   | Get character at index |
| `string-substr`  | `string start end` | Substring              |
| `string->symbol` | `string`           | Convert to symbol      |
| `symbol->string` | `symbol`           | Convert to string      |

**Examples:**

```scheme
(string-append "Hello" " " "World") ; → "Hello World"
(string-length "test")              ; → 4
(string-ref "abc" 1)                ; → #\b
(string-substr "hello" 1 4)         ; → "ell"
(string->symbol "my-var")           ; → my-var
(symbol->string 'my-var)            ; → "my-var"
```

## 🔗 **Lists and Pairs**

| Function              | Arguments    | Description       |
|-----------------------|--------------|-------------------|
| `cons`                | `item list`  | Create pair       |
| `car`                 | `pair`       | First element     |
| `cdr`                 | `pair`       | Rest of list      |
| `set-car!` `set-cdr!` | `pair value` | Modify pair       |
| `list`                | `item...`    | Create list       |
| `length`              | `list`       | List length       |
| `append`              | `list...`    | Concatenate lists |

**Examples:**

```scheme
(cons 1 '(2 3))          ; → (1 2 3)
(car '(a b c))           ; → a
(cdr '(a b c))           ; → (b c)
(list 1 2 3)             ; → (1 2 3)
(length '(a b c d))      ; → 4
(append '(1 2) '(3 4))   ; → (1 2 3 4)
```

## 🗃️ **Vectors**

| Function        | Arguments              | Description   |
|-----------------|------------------------|---------------|
| `vector`        | `item...`              | Create vector |
| `vector-ref`    | `vector integer`       | Get element   |
| `vector-set!`   | `vector integer value` | Set element   |
| `vector-length` | `vector`               | Vector size   |

**Examples:**

```scheme
(vector 1 2 3)                   ; → #(1 2 3)
(vector-ref #(a b c) 1)          ; → b
(vector-set! #(1 2 3) 1 99)      ; → #(1 99 3)
(vector-length #(a b c d))       ; → 4
```

## 🔍 **Type Predicates**

| Function     | Arguments         | Returns                |
|--------------|-------------------|------------------------|
| `null?`      | `value`           | #t if empty list       |
| `pair?`      | `value`           | #t if pair/list        |
| `symbol?`    | `value`           | #t if symbol           |
| `number?`    | `value`           | #t if integer or float |
| `string?`    | `value`           | #t if string           |
| `char?`      | `value`           | #t if character        |
| `vector?`    | `value`           | #t if vector           |
| `procedure?` | `value`           | #t if function         |
| `boolean?`   | `value`           | #t if #t or #f         |
| `type?`      | `type-name value` | Check specific type    |

**Examples:**

```scheme
(null? '())              ; → #t
(pair? '(1 2))           ; → #t
(symbol? 'x)             ; → #t
(number? 42)             ; → #t
(string? "hello")        ; → #t
(type? 'integer 5)       ; → #t
(type? 'string "test")   ; → #t
```

## ⚖️ **Comparisons**

| Function | Arguments | Description     |
|----------|-----------|-----------------|
| `eq?`    | `any any` | Object identity |
| `eqv?`   | `any any` | Value equality  |

**Examples:**

```scheme
(eq? 'a 'a)              ; → #t
(eq? '(1 2) '(1 2))      ; → #f (different objects)
(eqv? 5 5)               ; → #t
(eqv? "hi" "hi")         ; → #t (string value equality)
```

## 🗄️ **Hash Tables**

| Function             | Arguments         | Description              |
|----------------------|-------------------|--------------------------|
| `make-hash-table`    |                   | Create hash table        |
| `hash-table-set!`    | `table key value` | Set value                |
| `hash-table-ref`     | `table key`       | Get value                |
| `hash-table-try-ref` | `table key`       | Returns (success? value) |
| `hash-table?`        | `value`           | Check if hash table      |

**Examples:**

```scheme
(define ht (make-hash-table))
(hash-table-set! ht "name" "John")
(hash-table-ref ht "name")       ; → "John"
(hash-table-try-ref ht "age")    ; → (#f ())
```

## 💾 **File I/O**

| Function                     | Arguments               | top level     | Description                    |
|------------------------------|-------------------------|---------------|--------------------------------|
| `print` `pprint` `inspect`   | `value`                 |               | Print with different formats   |
| `fmt`                        | `#t format args...`     |               | Formatted output               |
|                              | `#f format args...`     |               | Formatt to string              |
| `error`                      | `message`               |               | Throw error                    |
| ---------------------------- | ----------------------- | -----------   | ------------------------------ |
| `file-exists?`               | `filename`              |               | Check if file exists           |
| ---------------------------- | ----------------------- | -----------   | ------------------------------ |
| `read-str`                   | `filename`              |               | Read from file to string       |
| `parse-str`                  | `string`                | yes           | Parse string                   |
| ---------------------------- | ----------------------- | -----------   | ------------------------------ |
| `read`                       | `reader`                |               | Read from string               |
| `read-char"`                 | `reader`                |               | Read from string               |
| `peek-char`                  | `reader`                |               | Read from string               |
| `read-delimited-list`        | `reader terminator`     |               | Read from string               |
| ---------------------------- | ----------------------- | -----------   | ------------------------------ |
| `load`                       | `filename`              | yes (execute) | Load and execute file          |
| `read-file`                  | `filename`              | yes           | Read file contents             |
| ---------------------------- | ----------------------- | -----------   | ------------------------------ |
| `set-reader-macro`           | `pattern replacement`   |               | Set reader macro               |
| `set-reader-macro`           | `pattern lambda`        |               | Set reader macro               |
| `remove-reader-mac`          | `pattern`               |               | Remove reader macro            |

**Examples:**

```scheme
(print "Hello")                    ; Print value
(load-file "script.sot")           ; Execute file
(file-exists? "data.txt")          ; → #t or #f
(fmt #t "Hello ~a" :arg1 "World")  ; Print formatted
```

## 🖥️ **System**

| Function                   | Arguments   | Description                         |
|----------------------------|-------------|-------------------------------------|
| `system`                   | `command`   | Execute shell command               |
| `get-environment-variable` | `name`      | Get env variable                    |
| `exit`                     |             | Exit interpreter                    |
| `get-path`                 | 'cwd        | Current working dir                 |
| `get-path`                 | 'home       | User home ~                         |
| `get-path`                 | 'config     | User settings ~/.config/soot/       |
| `get-path`                 | 'cache      | Shared files ~/.cache/soot/         |
| `get-path`                 | 'share      | Shared files /usr/local/share/soot/ |
| `get-path`                 | 'exec       | Binary folder of script             |
| `get-path`                 | 'project    | Project folder                      |
| `find-file`                | `file name` | Find file in the system directories |
|                            |             | The order is: project, user, system |

**Examples:**

```scheme
(system "ls -la")                     ; Execute command
(get-environment-variable "PATH")     ; Get PATH env
(current-directory)                   ; → "/current/path"
```

## 🔄 **Type Conversions**

| Function         | Arguments              | Description        |
|------------------|------------------------|--------------------|
| `number->string` | `number :base integer` | Convert to string  |
| `string->number` | `string :base integer` | Parse number       |
| `char->integer`  | `char`                 | Character to ASCII |
| `integer->char`  | `integer`              | ASCII to character |

**Examples:**

```scheme
(number->string 42 :base 16)     ; → "2a"
(string->number "FF" :base 16)   ; → 255
(char->integer #\A)              ; → 65
(integer->char 66)               ; → #\B
```

## 🎲 **Time Functions**

| Function            | Arguments | Description                                                  | Example Output        |
|---------------------|-----------|--------------------------------------------------------------|-----------------------|
| `time-seconds`      |           | Returns current Unix timestamp in seconds (since 1970-01-01) | `1734167895`          |
| `time-milliseconds` |           | Returns current Unix timestamp in milliseconds               | `1734167895123`       |
| `time-microseconds` |           | Returns current Unix timestamp in microseconds               | `1734167895123456`    |
| `time-nanoseconds`  |           | Returns current Unix timestamp in nanoseconds                | `1734167895123456789` |

## 🎲 **Other Functions**

| Function | Arguments    | Description            |
|----------|--------------|------------------------|
| `gensym` |              | Generate unique symbol |
| `eval`   | `expression` | Evaluate expression    |

**Examples:**

```scheme
(gensym)        ; → gensym0, gensym1, ...
(eval '(+ 1 2)) ; → 3
```

---

## 🚀 **Quick Start Examples**

### **Hello World**

```scheme
(print "Hello, World!")
```

### **Function Definition**

```scheme
(define (square x) (* x x))
(square 5)  ; → 25
```

### **List Processing**

```scheme
(map (lambda (x) (* x 2)) '(1 2 3 4))  ; → (2 4 6 8)
(filter even? '(1 2 3 4 5))            ; → (2 4)
```

### **File Processing**

```scheme
(define data (read-file "data.txt"))
(if (string? data)
    (print (string-length data))
    (error "Failed to read file"))
```

---

## ⚠️ **Common Pitfalls**

1. **Keywords need colon**: `:flag` not `flag` in function calls
2. **Lists vs. Vectors**: Use `()` for lists, `#()` for vectors
3. **Equality**: Use `eqv?` for value comparison, `eq?` for object identity
4. **No `&body`**: Use `&rest` instead

---

## 🔗 **Useful Patterns**

```scheme
;; Optional arguments with defaults
(lambda (&key (verbose #f) (level 1))
  (if verbose (print "Level:" level)))

;; Variable arguments
(lambda (&rest args)
  (apply + args))

;; Error handling (try/catch not built-in - use conditions)
(if (file-exists? "config.sot")
    (load-file "config.sot")
    (print "Config not found, using defaults"))
```

---

## 📖 **Type Reference**

| Type      | Literal    | Example               |
|-----------|------------|-----------------------|
| Integer   | `123`      | `42`, `-5`, `0xFF`    |
| Float     | `1.23`     | `3.14`, `-2.5`        |
| Boolean   | `#t`, `#f` | `#t`, `#f`            |
| String    | `"text"`   | `"Hello"`, `"test\n"` |
| Character | `#\c`      | `#\a`, `#\newline`    |
| Symbol    | `'name`    | `'x`, `'my-var`       |
| List      | `(a b c)`  | `'(1 2 3)`, `()`      |
| Vector    | `#(a b c)` | `#(1 2 3)`            |
| Pair      | `(a . b)`  | `(1 . 2)`             |

---

## 🎯 **REPL Commands**

```bash
sooti> (+ 1 2 3)        ; Evaluate expression
sooti> quit             ; Exit REPL
sooti> exit             ; Exit REPL
```

---

**Version**: SOOT Core [sha:...]  
**Syntax**: Scheme-like with Common Lisp influences  
**License**: Project-specific

## 📖 **Other Documentation**

- 📖 [SOOT Common Library Documentation](common/script/README.LIB.md)


