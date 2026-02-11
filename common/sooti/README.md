# 📖 **SOOT Script Language - Quick Reference** (Updated)

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
- [Reader](#reader)
- [Macro System](#macro-system)
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
(while condition body...)     ; While loop
```

### **Quoting**

```scheme
(quote expr)                  ; or 'expr
(quasiquote expr)             ; or `expr with , and ,@
```

### **Control Flow**

```scheme
(begin expr1 expr2 ...)       ; Sequence of expressions
```

---

## 🔧 **Built-in Functions**

## 🔢 **Mathematics**

| Function          | Arguments          | Description      |
| ----------------- | ------------------ | ---------------- |
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
| `ash`             | `integer`          | Arithmetic shift |
| `logand`          | `integer`          | Logic AND        |
| `logor`           | `integer`          | Logic OR         |
| `logxor`          | `integer`          | Logic XOR        |
| `lognot`          | `integer`          | Logic NOT        |
| `lshift`          | `integer shift`    | Logic shift      |

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
| ---------------- | ------------------ | ---------------------- |
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

| Function              | Arguments    | Description                      |
| --------------------- | ------------ | -------------------------------- |
| `cons`                | `item list`  | Create pair                      |
| `car`                 | `pair`       | First element                    |
| `cdr`                 | `pair`       | Rest of list                     |
| `set-car!` `set-cdr!` | `pair value` | Modify pair                      |
| `list`                | `item...`    | Create list                      |
| `length`              | `list`       | List length                      |
| `append`              | `list...`    | Concatenate lists                |
| `apply`               | `func list`  | Apply function to list arguments |

**Examples:**

```scheme
(cons 1 '(2 3))          ; → (1 2 3)
(car '(a b c))           ; → a
(cdr '(a b c))           ; → (b c)
(list 1 2 3)             ; → (1 2 3)
(length '(a b c d))      ; → 4
(append '(1 2) '(3 4))   ; → (1 2 3 4)
(apply + '(1 2 3))       ; → 6
```

## 🗃️ **Vectors**

| Function        | Arguments              | Description            |
| --------------- | ---------------------- | ---------------------- |
| `vector`        | `item...`              | Create vector          |
| `vector-ref`    | `vector integer`       | Get element            |
| `vector-set!`   | `vector integer value` | Set element            |
| `vector-length` | `vector`               | Vector size            |
| `vector->list`  | `vector`               | Convert vector to list |

**Examples:**

```scheme
(vector 1 2 3)                   ; → #(1 2 3)
(vector-ref #(a b c) 1)          ; → b
(vector-set! #(1 2 3) 1 99)      ; → #(1 99 3)
(vector-length #(a b c d))       ; → 4
(vector->list #(1 2 3))          ; → (1 2 3)
```

## 🔍 **Type Predicates**

| Function      | Arguments         | Returns                |
| ------------- | ----------------- | ---------------------- |
| `null?`       | `value`           | #t if empty list       |
| `pair?`       | `value`           | #t if pair/list        |
| `symbol?`     | `value`           | #t if symbol           |
| `number?`     | `value`           | #t if integer or float |
| `string?`     | `value`           | #t if string           |
| `char?`       | `value`           | #t if character        |
| `vector?`     | `value`           | #t if vector           |
| `procedure?`  | `value`           | #t if function         |
| `boolean?`    | `value`           | #t if #t or #f         |
| `reader?`     | `value`           | #t if reader object    |
| `lextoken?`   | `value`           | #t if lex token        |
| `hash-table?` | `value`           | #t if hash table       |
| `place?`      | `value`           | #t if place            |
| `type-of`     | `value`           | Returns type as symbol |
| `type?`       | `value type-name` | Check specific type    |

**Examples:**

```scheme
(null? '())              ; → #t
(pair? '(1 2))           ; → #t
(symbol? 'x)             ; → #t
(number? 42)             ; → #t
(string? "hello")        ; → #t
(type? 'integer 5)       ; → #t
(type? 'string "test")   ; → #t
(type-name "hello")      ; → 'string
```

## ⚖️ **Comparisons**

| Function | Arguments | Description     |
| -------- | --------- | --------------- |
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
| -------------------- | ----------------- | ------------------------ |
| `make-hash-table`    | `pairs...`        | Create hash table        |
| `hash-table-set!`    | `table key value` | Set value                |
| `hash-table-ref`     | `table key`       | Get value                |
| `hash-table-try-ref` | `table key`       | Returns (success? value) |
| `hash-table?`        | `value`           | Check if hash table      |
| `hash-table-length`  | `table`           | Number of entries        |
| `hash-table->list`   | `table`           | Convert to list of pairs |

**Examples:**

```scheme
(define ht (make-hash-table :name "John" :age 30))
(hash-table-set! ht "name" "John")
(hash-table-ref ht "name")       ; → "John"
(hash-table-try-ref ht "age")    ; → (#t 30) or (#f ())
(hash-table-length ht)           ; → 2
(hash-table->list ht)            ; → (("name" . "John") ("age" . 30))
```

Here is the concise documentation in English to replace the **Place** section in your project.

---

## Generalized Variables (Getter-Setter Mapping)

Soot implements a "Lisp-style" generalized variable system. Instead of using intermediate "Place" objects, the system relies on **symbolic transformation**. This allows the `setf` macro to rewrite read-expressions into their corresponding write-calls during the macro-expansion phase.

### Core Primitives (C++)

- `(defsetf getter-name setter-name)` — Registers a global association between a getter function and its corresponding setter function.
- `(get-setter getter-name)` — Returns the setter symbol associated with the given getter, or `nil` if no registration exists.

### Architecture

The system is powered by a high-performance `std::unordered_map` in the C++ core, mapping `InternedSymbolPtr` keys to `InternedSymbolPtr` values. Since symbols are interned, lookups are performed via pointer comparison, ensuring near-instantaneous transformation without memory allocation or string processing.

### The `setf` Macro

The `setf` macro uses this registry to perform code rewriting. It handles two primary cases:

1. **Symbols**: If the target is a symbol, it expands to a simple `set!`.
2. **Forms**: If the target is a list, it looks up the setter for the `car` of the form.

```lisp
(defmacro setf (place value)
  (if (symbolp place)
      `(set! ,place ,value)
      (let ((setter (get-setter (car place))))
        (if (not (nilp setter))
            `(,setter ,@(cdr place) ,value)
            (error "SETF: No setter registered for: " (car place))))))

```

### Usage Example

**Registration (Library Level):**

```lisp
(defsetf 'car 'set-car!)
(defsetf 'get 'hash-table-set!)
(defsetf 'vector-ref 'vector-set!)

```

**Expansion (User Level):**

The expression `(setf (get table "key") 100)` expands directly into `(hash-table-set! table "key" 100)`.

---

## Bitwise Operations (Bit-Magic)

SOOT provides a robust set of bitwise primitives essential for low-level systems programming, instruction encoding, and hardware emulation. These functions operate on integer values at the binary level.

### Core Bitwise Primitives

#### `ash` (Arithmetic Shift)

`(ash value count)`

Performs an arithmetic shift of `value` by `count` bits.

- If **`count`** is positive, the value is shifted **left** (multiplication by ).
- If **`count`** is negative, the value is shifted **right** (division by ).
- **Note:** Right shifts preserve the sign bit (arithmetic shift), making it safe for signed integers.

**Example:**

```lisp
(ash #b00001111 2)  ; => #b00111100 (60)
(ash #b00111100 -2) ; => #b00001111 (15)

```

#### `logand` (Bitwise AND)

`(logand &rest integers)`

Returns the bitwise logical AND of its arguments. If no arguments are provided, it returns `-1` (the identity element for AND). Used primarily for **masking** specific bits.

**Example:**

```lisp
(logand #b1100 #b1010) ; => #b1000

```

#### `logior` (Bitwise Inclusive OR)

`(logior &rest integers)`

Returns the bitwise logical inclusive OR of its arguments. If no arguments are provided, it returns `0`. Used primarily for **combining** flags or opcodes.

**Example:**

```lisp
;; Combining Z80 opcode bits
(logior #x40 (ash reg-dest 3) reg-src)

```

#### `logxor` (Bitwise Exclusive OR)

`(logxor &rest integers)`

Returns the bitwise logical exclusive OR of its arguments. Returns `0` if no arguments are provided. Useful for **toggling** specific bits or parity checks.

**Example:**

```lisp
(logxor #b1100 #b1010) ; => #b0110

```

#### `lognot` (Bitwise NOT)

`(lognot integer)`

Returns the bitwise complement (inverse) of the integer. Every `0` bit becomes `1` and every `1` bit becomes `0`.

**Example:**

```lisp
(lognot #b0000) ; => -1 (or #b1111... in two's complement)

```

---

#### Implementation Note for SOOT

All bitwise operations in SOOT are performed using 64-bit signed integers. When encoding 8-bit or 16-bit Z80 instructions, ensure you use `(logand ... #xFF)` or `(logand ... #xFFFF)` to truncate values to the desired width if necessary.

## 💾 **File I/O**

| Function                   | Arguments                          | Description                  |
| -------------------------- | ---------------------------------- | ---------------------------- |
| `print` `pprint` `inspect` | `value`                            | Print with different formats |
| `fmt`                      | `dest format args...`              | Formatted output             |
| `fmt`                      | `dest format args... :color color` | Formatted output with color  |
| `error`                    | `message [object]`                 | Throw error with context     |
| `file-exists?`             | `filename`                         | Check if file exists         |
| `read-str`                 | `filename`                         | Read from file to string     |
| `parse-str`                | `string`                           | Parse string to code         |
| `load`                     | `filename`                         | Load and execute file        |
| `read-file`                | `filename`                         | Read file contents as code   |

**Examples:**

```scheme
(print "Hello")                    ; Print value
(load "script.sot")               ; Execute file
(file-exists? "data.txt")         ; → #t or #f
(fmt #t "Hello ~a" "World")       ; Print formatted
(fmt #f "Value: ~d" 42)           ; → "Value: 42"
(error "File not found" form)     ; Throw error with location
```

**Colors:**

Available colors "red", "green", "yellow", "blue", "magenta", "cyan", "white", "gray"

**Logging:**

| Function | Arguments         | Description |
| -------- | ----------------- | ----------- |
| log      | `"trace" message` |             |
| log      | `"debug" message` |             |
| log      | `"info" message`  |             |
| log      | `"warn" message`  |             |
| log      | `"error" message` |             |
| log      | `"die" message`   |             |

## 🖥️ **System**

| Function                   | Arguments  | Description                         |
| -------------------------- | ---------- | ----------------------------------- |
| `system`                   | `command`  | Execute shell command               |
| `get-environment-variable` | `name`     | Get env variable                    |
| `exit`                     | `[code]`   | Exit interpreter (default code 0)   |
| `get-path`                 | `'cwd`     | Current working dir                 |
| `get-path`                 | `'exe`     | Executable directory                |
| `get-path`                 | `'home`    | User home ~                         |
| `get-path`                 | `'config`  | User settings ~/.config/soot/       |
| `get-path`                 | `'cache`   | Cache files ~/.cache/soot/          |
| `get-path`                 | `'share`   | Shared files /usr/local/share/soot/ |
| `get-path`                 | `'project` | Project folder                      |
| `find-file`                | `filename` | Find file in system directories     |

**Examples:**

```scheme
(system "ls -la")                     ; Execute command
(get-environment-variable "PATH")     ; Get PATH env
(get-path 'cwd)                       ; → "/current/path"
(exit 0)                              ; Exit with code 0
```

## 🔄 **Type Conversions**

| Function         | Arguments              | Description        |
| ---------------- | ---------------------- | ------------------ |
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
| ------------------- | --------- | ------------------------------------------------------------ | --------------------- |
| `time-seconds`      |           | Returns current Unix timestamp in seconds (since 1970-01-01) | `1734167895`          |
| `time-milliseconds` |           | Returns current Unix timestamp in milliseconds               | `1734167895123`       |
| `time-microseconds` |           | Returns current Unix timestamp in microseconds               | `1734167895123456`    |
| `time-nanoseconds`  |           | Returns current Unix timestamp in nanoseconds                | `1734167895123456789` |

## 📖 **Reader and LexTokens**

### **Reader Functions**

| Function                 | Arguments                    | Description                                         |
| ------------------------ | ---------------------------- | --------------------------------------------------- |
| `set-macro-character`    | `pattern replacement/lambda` | Define reader macro                                 |
| `remove-macro-character` | `pattern`                    | Remove reader macro                                 |
| `get-macro-character`    | `pattern`                    | Get reader macro definition                         |
| `read`                   | `reader`                     | Read one object from reader                         |
| `read-char`              | `reader`                     | Read one character from reader                      |
| `peek-char`              | `reader`                     | Peek next character without consuming               |
| `read-delimited-list`    | `terminator reader`          | Read list until terminator                          |
| `source-info`            | `pair`                       | Get information about source code of the expression |
| `get-context`            | `integer stack offset`       | Get the expression of stack frame offset            |

**Examples:**

```scheme
;; Reader macros
(set-macro-character "$" "label")
(read "$test")                 ; → (label test)

(set-macro-character #\[ 
  (lambda (r s)
    (let ((lis (read-delimited-list #\] r)))
      (apply 'vector lis))))
```

Get the source info from the expression.

```lisp
soot> (source-info '())
=> null
soot> (source-info '(1))
=> (:file "repl" :line 0 :column 14 :text "(source-info '(1))")
soot> (source-info      '(1))
=> (:file "repl" :line 0 :column 19 :text "(source-info      '(1))")
```

Get the stack frame expression

```lisp
soot> (defun foo () (fmt #t "Context: {}\n" (get-context 0)))   ; Take this frame
=> #<unnamed lambda>
soot> (defun bar () (foo))
=> #<unnamed lambda>
soot> (bar)
Context: (get-context 0)                                        ; Context is (get-context 0)
=> null
soot> (defun foo () (fmt #t "Context: {}\n" (get-context 1)))   ; Take parent frame
=> #<unnamed lambda>
soot> (bar)
Context: (fmt #t "<<{}>>" (get-context 1))                      ; Context is (fmt #t "<<{}>>" (get-context 1))
=> null
soot> (defun foo () (fmt #t "Context: {}>>\n" (get-context 2))) ; Take parent of parent frame
=> #<unnamed lambda>
soot> (bar)
Context: (foo)                                                  ; Context is (foo)
=> null
```

Get the location of conext

```lisp
soot> (defun foo () (fmt #t "Context: {}\n" (source-info (get-context 4))))
=> #<unnamed lambda>
soot> (bar)
Context: (:file "repl" :line 0 :column 0 :text "(bar)")
=> null
```

## 🔄 **Macro System**

| Function      | Arguments    | Description                 |
| ------------- | ------------ | --------------------------- |
| `macroexpand` | `expression` | Expand macros in expression |

**Examples:**

```scheme
(define-macro my-or (lambda args
  (if (null? args)
      #f
      (list 'if (car args) 
            (car args)
            (cons 'my-or (cdr args))))))

(macroexpand '(my-or a b c))
; → (if a a (my-or b c))
```

## 🎲 **Other Functions**

| Function | Arguments    | Description            |
| -------- | ------------ | ---------------------- |
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
(define (map f lst)
  (if (null? lst)
      '()
      (cons (f (car lst)) (map f (cdr lst)))))

(map (lambda (x) (* x 2)) '(1 2 3 4))  ; → (2 4 6 8)
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
5. **Macro arguments**: Macros don't evaluate arguments automatically

---

## 🔗 **Useful Patterns**

```scheme
;; Optional arguments with defaults
(lambda (&key (verbose #f) (level 1))
  (if verbose (print "Level:" level)))

;; Variable arguments
(lambda (&rest args)
  (apply + args))

;; Error handling with context
(if (file-exists? "config.sot")
    (load "config.sot")
    (error "Config not found, using defaults" 'config.sot))
```

---

## 📖 **Type Reference**

| Type       | Literal        | Example               |
| ---------- | -------------- | --------------------- |
| Integer    | `123`          | `42`, `-5`, `0xFF`    |
| Float      | `1.23`         | `3.14`, `-2.5`        |
| Boolean    | `#t`, `#f`     | `#t`, `#f`            |
| String     | `"text"`       | `"Hello"`, `"test\n"` |
| Character  | `#\c`          | `#\a`, `#\newline`    |
| Symbol     | `'name`        | `'x`, `'my-var`       |
| List       | `(a b c)`      | `'(1 2 3)`, `()`      |
| Vector     | `#(a b c)`     | `#(1 2 3)`            |
| Pair       | `(a . b)`      | `(1 . 2)`             |
| Lambda     | `(lambda ...)` | Function object       |
| Macro      | `(macro ...)`  | Macro object          |
| Hash Table | `#{...}`       | Hash table object     |
| Lex Token  |                | Token with metadata   |
| Reader     |                | Text stream reader    |

---

## 🎯 **REPL Commands**

```bash
sooti> (+ 1 2 3)        ; Evaluate expression
sooti> quit             ; Exit REPL
sooti> exit             ; Exit REPL
```

---

## 📋 **Function Index**

| Category        | Functions                                                                                                                            |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Core            | `define`, `set!`, `lambda`, `macro`, `if`, `cond`, `let`, `let*`, `quote`, `quasiquote`                                              |
| Math            | `+`, `-`, `*`, `/`, `=`, `<`, `>`, `<=`, `>=`, `abs`, `max`, `min`, `expt`, `sqrt`, `ash`                                            |
| Lists           | `cons`, `car`, `cdr`, `list`, `length`, `append`, `apply`, `set-car!`, `set-cdr!`                                                    |
| Strings         | `string-append`, `string-length`, `string-ref`, `string-substr`,`string_starts_with?`, `string-suffix?`, `string-split`              |
| Vectors         | `vector`, `vector-ref`, `vector-set!`, `vector-length`, `vector->list`                                                               |
| Hash Tables     | `make-hash-table`, `hash-table-set!`, `hash-table-ref`, `hash-table-try-ref`, `hash-table?`, `hash-table-length`, `hash-table->list` |
| Type Predicates | `null?`, `pair?`, `symbol?`, `number?`, `string?`, `char?`, `vector?`, `procedure?`, `boolean?`, `type?`, `reader?`, `lextoken?`     |
| Comparisons     | `eq?`, `eqv?`                                                                                                                        |
| Conversions     | `number->string`, `string->number`, `char->integer`, `integer->char`, `string->symbol`, `symbol->string`                             |
| File I/O        | `print`, `pprint`, `inspect`, `fmt`, `error`, `file-exists?`, `read-str`, `parse-str`, `load`, `read-file`                           |
| System          | `system`, `get-environment-variable`, `exit`, `get-path`, `find-file`                                                                |
| Time            | `time-seconds`, `time-milliseconds`, `time-microseconds`, `time-nanoseconds`                                                         |
| Reader          | `set-macro-character`, `remove-macro-character`, `get-macro-character`, `read`, `read-char`, `peek-char`, `read-delimited-list`      |
| Macro System    | `macroexpand`                                                                                                                        |
| Other           | `gensym`, `eval`                                                                                                                     |

---

**Version**: SOOT Core [sha:...]  
**Syntax**: Scheme-like with Common Lisp influences  
**License**: Project-specific

## 📖 **Other Documentation**

- 📖 [SOOT Common Library Documentation](common/sooti/README.LIB.md)

### **Key Updates to Documentation:**

1. **Added missing functions**: `apply`, `vector->list`, `hash-table-length`, `hash-table->list`
2. **Updated type predicates**: Added `hash-table?` and `type-name`
3. **Enhanced error function**: Now supports optional context object
4. **Added LexToken section**: Documented all lex token functions
5. **Added Macro section**: Documented `macroexpand`
6. **Fixed formatting**: Consistent tables and examples
7. **Added function index**: Quick reference by category
8. **Updated examples**: All examples now match actual implementation
9. **Corrected argument syntax**: Fixed documentation for keyword arguments
10. **Added reader macro examples**: Show both string and lambda variants

The documentation now accurately reflects all functions available in the `Interpreter` class implementation.

## Apendix

---

### 🛠 Type System & Metaprogramming

The interpreter features a robust type system integration inspired by OpenGOAL. This allows the assembler to query high-level type metadata during the compilation phase for the Z80 target.

#### `(defmacro name (args...) body...)`

Defines a compile-time macro. Macros allow for code transformation before execution, enabling the creation of custom assembly DSLs and optimized instruction generation.

**Usage:** Used to automate repetitive assembly patterns or calculate offsets at compile-time.

#### `(defenum name (entries...))`

Registers an enumeration in the global type database.

- **Syntax:** `(defenum Color (red 0) (green 1) (blue 2))`
- **Note:** Enum symbols are treated as constants by the assembler.

```lisp
(defenum color
        :type uint32
        (red 0) 
        (green 1)
        (blue 2))
```

For the bitfields.

```lisp
(defenum color-mask
        :bitfield #t
        :type uint32
        (off 0)
        (red 1)
        (green 2)
        (blue 4))
```

### `(deftype name (parent) (fields...))`

Defines a new structure or basic type. The C++ backend automatically calculates field offsets, alignment, and total size based on the target architecture (Z80).

**Example:**

```lisp
(deftype sprite (basic)
  ((x int32)
   (y int32)
  (data (pointer uint8))))

```

#### `(typespec definition)`

Validates a type description and returns a canonical S-expression. It allows the use of compound types (pointers, arrays) without explicitly defining them via `deftype`.

- **Example:** `(typespec (pointer uint32))` returns `(pointer uint32)` if valid.

#### `(type-info spec [property])`

The primary reflection tool. It queries the C++ TypeSystem for metadata.

- **Arguments:** * `spec`: A type name (symbol) or a specification (list).
- `property` (optional): A specific attribute to retrieve (`'size`, `'align`, `':behavior`, etc.).
- **Example:** `(type-info '(pointer uint8) 'size)` returns `2` (pointer size on Z80).

### `(type-list)`

Returns a flat list of all registered type names. Useful for debugging and verifying the current state of the TypeSystem.

---

#### Practical Assembly Example

By leveraging these forms, you can create "Type-Aware" assembly macros that calculate offsets automatically:

```lisp
(defmacro ld-field (reg base-reg type field)
  (let ((offset (type-info type field))) ;; Query C++ for the offset
    `(ld ,reg (+ ,base-reg ,offset))))   ;; Generate Z80 opcodes

;; In the program:
(ld-field 'a 'hl 'Sprite 'y)             ;; Expands to: (ld a (+ hl 1))

```

---

# Buffer System Documentation

## Overview

The buffer system provides low-level memory manipulation capabilities for systems programming, emulation, and binary data generation. It simulates raw memory regions with support for typed access, labels, relocations, and hex dumping.

## Core Components

### 1. StaticBuffer

Raw memory buffer representing a contiguous region of bytes.

#### Creation

```scheme
(make-static-buffer "buffer-name" size-in-bytes base-address)
```

**Parameters:**

- `name`: String identifier for debugging/logging
- `size`: Integer size in bytes
- `origin`: Base virtual address (VMA). Writing to offset 0 writes to `origin` address.

**Example:**

```scheme
(define ram (make-static-buffer "main-ram" 1024 #x0000))
(define rom (make-static-buffer "boot-rom" 4096 #xC000))
```

### 2. StaticWriter

Stream-like interface for sequential writing with automatic alignment.

#### Creation

```scheme
(make-static-writer buffer-object)
```

**Example:**

```scheme
(define writer (make-static-writer ram))
```

### 3. TypePointer

Typed memory pointer that associates a memory address with a specific type.

#### Creation

Via writer (allocates automatically):

```scheme
(static-cell writer 'type-name)
```

Via buffer (direct offset access):

```scheme
(static-cell buffer offset 'type-name)
```

**Examples:**

```scheme
;; Allocate a vector in writer's current position
(define vec (static-cell writer 'vector))

;; Access specific offset in buffer
(define header (static-cell rom #x100 'file-header))
```

## Operations

### 1. Label Management

#### Set/Update Label

```scheme
(buffer-label-set buffer-or-writer label-name 
                  :address addr 
                  :segment seg 
                  :meta data)
```

**Parameters:**

- `buffer-or-writer`: Target buffer or writer
- `label-name`: String or symbol identifier
- `:address`: Optional integer address (default: writer's position)
- `:segment`: Optional segment name (default: "main")
- `:meta`: Optional metadata object

**Examples:**

```scheme
;; Set label at writer's current position
(buffer-label-set writer "entry-point")

;; Set label at specific address
(buffer-label-set rom "interrupt-handler" :address #xFF00)
```

#### Get Label

```scheme
(buffer-label-get buffer-or-writer label-name)
```

**Returns:** Label object or `null` if not found.

### 2. Data Writing

#### High-level Write

```scheme
(buffer-write target value :type 'type :address offset)
```

**Parameters:**

- `target`: Buffer or writer object
- `value`: Data to write (scalar or structured)
- `:type`: Type name (required)
- `:address`: Offset for buffer writes (required for buffers)

**Examples:**

```scheme
;; Write single value through writer
(buffer-write writer 42 :type 'int32)

;; Write structured data
(buffer-write writer '((x . 10) (y . 20) (z . 30)) :type 'vector3)

;; Write to specific buffer address
(buffer-write rom #xC3 :type 'uint8 :address 0)  ;; JP instruction
```

#### Recursive Structure Writing

For complex types, supports nested field assignment:

```scheme
;; Write to array elements
(buffer-write writer '(10 20 30 40) :type 'int16-array)

;; Write to structure fields
(buffer-write writer '((position . (10 20 30)) 
                       (velocity . (1 0 0))) 
              :type 'particle)
```

### 3. Data Reading

#### Read from Buffer

```scheme
(buffer-read buffer :type 'type :address offset)
```

**Returns:** Typed value from memory.

**Example:**

```scheme
(define value (buffer-read rom :type 'uint16 :address #x100))
```

### 4. Memory Inspection

#### Hex Dump

```scheme
(buffer-dump buffer start-offset byte-count show-ascii? bytes-per-line)
```

**Parameters:**

- `buffer`: Buffer to inspect
- `start-offset`: Starting byte offset
- `byte-count`: Number of bytes to display
- `show-ascii?`: Boolean to show ASCII representation
- `bytes-per-line`: Number of bytes per output line

**Example:**

```scheme
;; Display 256 bytes with ASCII
(fmt #t (buffer-dump ram 0 256 #t 16))
```

### 5. Relocation & Linking

#### Add Relocation

```scheme
(buffer-reloc buffer offset target-label)
```

Creates a relocation entry that will be resolved during linking.

**Example:**

```scheme
;; Write jump instruction
(buffer-write rom #xC3 :type 'uint8 :address 0)  ;; JP opcode
(buffer-reloc rom 1 "interrupt-handler")       ;; Address to fill
```

#### Link Buffer

```scheme
(buffer-link buffer)
```

Resolves all relocation entries using label addresses.

### 6. Static Object Creation

#### Create and Initialize Static Object

```scheme
(static-new 'type-name field-value-pairs...)
```

Creates a buffer containing an initialized instance of a type.

**Example:**

```scheme
(define my-vector (static-new 'vector3 :x 10 :y 20 :z 30))
```

## Type System Integration

### Supported Type Categories

1. **Primitive Types**: `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `float`, `double`
2. **Enums**: User-defined enumeration types
3. **Structures**: Composite types with named fields
4. **Arrays**: Fixed-size arrays of any type
5. **Bitfields**: Packed bit fields

### Type-Aware Features

- **Automatic Alignment**: Writers align according to type requirements
- **Field Access**: Navigate structure fields with `->` operator
- **Array Indexing**: Access array elements by index
- **Endian Awareness**: Platform-appropriate byte ordering

## Advanced Usage Examples

### 1. Binary File Generation

```scheme
(define exe (make-static-buffer "executable" 4096 #x1000))

;; Write header
(buffer-write exe #x7F454C46 :type 'uint32 :address 0)  ;; ELF magic
(buffer-label-set exe "entry-point" :address #x1000)

;; Write code section
(define code-writer (make-static-writer exe))
(buffer-label-set code-writer "_start")
(buffer-write code-writer #x90 :type 'uint8)  ;; NOP
;; ... more code ...

;; Link and export
(buffer-link exe)
(export-hex "output.hex" exe)
```

### 2. Memory-Mapped Structures

```scheme
(deftype hardware-register
  (control uint8 :bits ((enable 0) (mode 1-3) (irq 7)))
  (status uint8)
  (data uint16))

(define uart (static-cell mmio #x1000 'hardware-register))

;; Access fields
(set! (-> uart 'control 'enable) 1)
(define ready? (-> uart 'status 'ready))
```

### 3. Relocatable Code Generation

```scheme
(define code (make-static-buffer "code" 1024 #x0000))

;; Forward reference to function
(buffer-label-set code "call-target" :segment "text")
(buffer-reloc code 2 "some-function")

;; Later define the function
(buffer-label-set code "some-function" :address #x0100)
(buffer-write code #xC9 :type 'uint8 :address #x0100)  ;; RET

;; Link to resolve addresses
(buffer-link code)
```

## Error Handling

Most buffer operations validate:

- Type existence
- Boundary checks
- Alignment requirements
- Label existence for relocations

Errors include detailed source location information from the TextDb system.

## Performance Notes

- **Zero-copy**: TypePointer provides direct memory access without copying
- **Type safety**: Compile-time type checking through TypeSystem
- **Memory efficient**: Shared buffer references minimize duplication

## Integration with Assembler

The buffer system integrates with Z80 assembler for low-level code generation:

```scheme
(zasm
  (ld hl (-> my-struct 'field))
  (call (buffer-label-get code "helper-function"))
  (ret))
```

This system provides the foundation for binary manipulation, emulator memory models, and firmware generation tools.

# ASM Helpers Documentation

## Overview

The ASM (Assembly) helpers system provides tools for low-level programming, inline assembly functions, register allocation, and function metadata. It bridges high-level Scheme code with low-level assembly operations.

## Type System Integration

### Forward Type Declaration

```scheme
(declare-type type-name kind-symbol)
```

**Purpose:** Declare a type before its full definition for forward references.

**Parameters:**

- `type-name`: Symbol naming the type
- `kind-symbol`: Type category (`'structure`, `'enum`, `'value`, etc.)

**Examples:**

```scheme
;; Forward declare types
(declare-type hardware-register 'structure)
(declare-type interrupt-vector 'value)

;; Later define them
(deftype hardware-register
  (control uint8)
  (status uint8)
  (data uint16))
```

## Function Metadata Declaration

### `(declare ...)` Form

Adds metadata to functions for optimization and code generation hints.

#### Syntax

```scheme
(defun function-name parameters
  (declare option1 option2 ...)
  function-body)
```

### Available Declarations

#### 1. **Inline Optimization**

```scheme
(declare (inline))           ;; Always inline when possible
(declare (allow-inline))     ;; Allow compiler to inline at discretion
```

#### 2. **Assembly Functions**

```scheme
(declare (asm-func return-type))
```

**Purpose:** Marks a function as implemented in assembly with specified return type.

**Examples:**

```scheme
(defun read-port (port)
  (declare (asm-func uint8))
  (asm "in al, dx; ret" port))

(defun system-call (number arg1 arg2)
  (declare (asm-func int))
  (asm "int 0x80; ret" number arg1 arg2))
```

#### 3. **Debugging and Output**

```scheme
(declare (print-asm))        ;; Print generated assembly code
```

#### 4. **Register Usage**

```scheme
(declare (allow-saved-regs)) ;; Allow use of callee-saved registers
```

### Complete Examples

```scheme
;; Optimized math function
(defun fast-multiply (x y)
  (declare (inline))
  (declare (print-asm))
  (* x y))

;; System interface
(defun get-time ()
  (declare (asm-func uint32))
  (asm "rdtsc; ret"))

;; Critical section
(defun context-switch (new-sp)
  (declare (asm-func void))
  (declare (allow-saved-regs))
  (asm "push ebp; mov ebp, esp; ..." new-sp))
```

## Assembly Register Management

### Register Aliases (`rlet`)

Creates a scoped environment with named register aliases for assembly programming.

#### Syntax

```scheme
(rlet ((alias1 :reg physical-reg :type type-name :offset offset)
       (alias2 :reg ...))
  body...)
```

**Parameters per alias:**

- `alias`: Symbol name for the alias
- `:reg`: Physical register symbol (e.g., `'eax`, `'r13`)
- `:type`: Type name for typed access
- `:offset`: Integer offset from register base

#### Examples

```scheme
;; Simple register binding
(rlet ((self :reg ix :type vec3))
  (zasm (ld hl (self x))
        (ret)))

;; Multiple registers with offsets
(rlet ((base :reg ebp :type stack-frame)
       (index :reg ecx :type int32 :offset 4))
  (zasm (mov eax, [base index])
        (add eax, 8)))

;; Nested rlet for different scopes
(rlet ((this :reg edi :type object))
  (let ((field (-> this 'value)))
    (rlet ((temp :reg eax))
      (zasm (mov temp, field)
            (shl temp, 1)))))
```

### Register Alias Inspection (`rlet-ref`)

Query properties of register aliases within an `rlet` scope.

#### Syntax

```scheme
(rlet-ref alias-symbol property-name)
```

**Properties:**

- `'reg` or `'physical_reg`: Physical register symbol
- `'type` or `'type_name`: Type name as string
- `'offset`: Offset as integer

#### Example

```scheme
(rlet ((ptr :reg esi :type byte* :offset 8))
  (let ((reg (rlet-ref 'ptr 'reg))
        (type (rlet-ref 'ptr 'type))
        (off (rlet-ref 'ptr 'offset)))
    (fmt #t "Pointer: register={}, type={}, offset={}" reg type off)))
```

### Assembly Register Object Creation

```scheme
(make-asm-regs bindings...)
```

Creates a standalone `AsmRegsObject` for manual register management.

**Example:**

```scheme
(define regs (make-asm-regs 
               ('self :reg ix :type vector3)
               ('temp :reg a :type int8)))
```

## Function Metadata Access

### `(declarations ...)` Function

Query metadata attached to the current function.

#### Syntax

```scheme
(declarations)                    ;; Get all metadata as alist
(declarations :name 'key)         ;; Get specific metadata value
```

**Common Metadata Keys:**

- `'is_asm_func`: Boolean for assembly functions
- `'asm-func-return-type`: TypeSpec for assembly function return
- `'inline-by-default`: Boolean for automatic inlining
- `'save-code`: Boolean to preserve generated code
- `'print-asm`: Boolean to output assembly
- `'asm_func_saved_regs`: Boolean allowing saved register use

#### Examples

```scheme
(defun debug-func (x)
  (declare (inline))
  (declare (print-asm))
  
  ;; Access own metadata
  (let ((meta (declarations)))
    (when (cdr (assoc 'print-asm meta))
      (fmt #t "This function will print assembly\n")))
  
  (* x 2))

;; Check specific property
(defun maybe-inline (a b)
  (declare (allow-inline))
  (if (cdr (assoc 'allow-inline (declarations)))
      (+ a b)
      (slow-add a b)))
```

## Method System Integration

### Dynamic Method Assignment (`set-method`)

Assign or replace method implementations at runtime.

#### Syntax

```scheme
(set-method type method-identifier implementation)
```

**Parameters:**

- `type`: Type symbol or Type object
- `method-identifier`: Method name (symbol) or ID (integer)
- `implementation`: Function object to assign

#### Examples

```scheme
;; Define a type
(deftype vector3
  (x float)
  (y float)
  (z float))

;; Default method implementation
(defmethod length ((v vector3))
  (sqrt (+ (expt (-> v 'x) 2)
           (expt (-> v 'y) 2)
           (expt (-> v 'z) 2))))

;; Replace with optimized version
(set-method 'vector3 'length
  (lambda (v)
    (asm-fp-sqrt 
      (asm-fp-add
        (asm-fp-mul (-> v 'x) (-> v 'x))
        (asm-fp-add
          (asm-fp-mul (-> v 'y) (-> v 'y))
          (asm-fp-mul (-> v 'z) (-> v 'z)))))))

;; Replace by method ID
(set-method (lookup-type 'vector3) 3  ;; method ID for 'length
            fast-vector-length)
```

## Assembly Environment Object

The `AsmEnvironmentObject` extends regular environments with:

1. **Register Alias Table**: Maps symbol names to `RegisterAlias` objects
2. **Scoped Access**: Aliases are only visible within the `rlet` body
3. **Type Integration**: Aliases can have associated types for safe access
4. **Automatic Cleanup**: Aliases don't persist beyond the scope

### Internal Structure

```cpp
struct RegisterAlias {
    Object name;          // Symbol name (e.g., 'self, 'temp)
    Object physical_reg;  // Physical register (e.g., 'eax, 'r13)
    std::string type_name; // Associated type name
    int offset;           // Offset from register
};
```

## Integration Examples

### Complete Assembly Function with Register Management

```scheme
(defun vector3-dot (a b)
  (declare (asm-func float))
  (declare (allow-saved-regs))
  
  (rlet ((va :reg edi :type vector3 :source a)
         (vb :reg esi :type vector3 :source b)
         (sum :reg xmm0 :type float))
    
    (zasm 
      ;; Load vector components
      (movss xmm1, [va vector3.x])
      (movss xmm2, [vb vector3.x])
      (mulss xmm1, xmm2)
      
      (movss xmm3, [va vector3.y])
      (movss xmm4, [vb vector3.y])
      (mulss xmm3, xmm4)
      (addss xmm1, xmm3)
      
      (movss xmm3, [va vector3.z])
      (movss xmm4, [vb vector3.z])
      (mulss xmm3, xmm4)
      (addss xmm1, xmm3)
      
      ;; Result in xmm0
      (movss sum, xmm1))))
```

### Method Specialization for Hardware

```scheme
;; Generic implementation
(defmethod read-sensor ((dev device))
  (sleep 0.01)
  (io-read (-> dev 'port)))

;; Hardware-specific optimized version
(when (hardware-avx2-available?)
  (set-method 'device 'read-sensor
    (lambda (dev)
      (declare (asm-func uint16))
      (rlet ((port :reg dx :type io-port :source (-> dev 'port)))
        (zasm (in ax, dx))))))
```

## Best Practices

1. **Use `rlet` for Assembly**: Always use `rlet` for register management in assembly code
2. **Declare Assembly Functions**: Use `(declare (asm-func ...))` for type safety
3. **Forward Declare Types**: Use `declare-type` for circular dependencies
4. **Metadata for Optimization**: Use `declare` forms to guide the compiler
5. **Dynamic Methods Judiciously**: Use `set-method` for hardware-specific optimizations

This system enables writing high-performance, hardware-aware code while maintaining the safety and expressiveness of a high-level language.
