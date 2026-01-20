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
- [Reader and LexTokens](#reader-and-lextokens)
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

| Function            | Arguments              | Description            |
|---------------------|------------------------|------------------------|
| `vector`            | `item...`              | Create vector          |
| `vector-ref`        | `vector integer`       | Get element            |
| `vector-set!`       | `vector integer value` | Set element            |
| `vector-length`     | `vector`               | Vector size            |
| `vector->list`      | `vector`               | Convert vector to list |

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
|---------------|-------------------|------------------------|
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

| Function                 | Arguments         | Description              |
|--------------------------|-------------------|--------------------------|
| `make-hash-table`        | `pairs...`        | Create hash table        |
| `hash-table-set!`        | `table key value` | Set value                |
| `hash-table-ref`         | `table key`       | Get value                |
| `hash-table-try-ref`     | `table key`       | Returns (success? value) |
| `hash-table?`            | `value`           | Check if hash table      |
| `hash-table-length`      | `table`           | Number of entries        |
| `hash-table->list`       | `table`           | Convert to list of pairs |

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
|----------------------------|------------------------------------|------------------------------|
| `print` `pprint` `inspect` | `value`                            | Print with different formats |
| `fmt`                      | `dest format args...`              | Formatted output             |
| `cfmt`                     | `dest format args... :color color` | Formatted output with color  |
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
|----------|-------------------|-------------|
| log      | `"trace" message` |             |
| log      | `"debug" message` |             |
| log      | `"info" message`  |             |
| log      | `"warn" message`  |             |
| log      | `"error" message` |             |
| log      | `"die" message`   |             |

## 🖥️ **System**

| Function                   | Arguments      | Description                         |
|----------------------------|----------------|-------------------------------------|
| `system`                   | `command`      | Execute shell command               |
| `get-environment-variable` | `name`         | Get env variable                    |
| `exit`                     | `[code]`       | Exit interpreter (default code 0)   |
| `get-path`                 | `'cwd`         | Current working dir                 |
| `get-path`                 | `'exe`         | Executable directory                |
| `get-path`                 | `'home`        | User home ~                         |
| `get-path`                 | `'config`      | User settings ~/.config/soot/       |
| `get-path`                 | `'cache`       | Cache files ~/.cache/soot/          |
| `get-path`                 | `'share`       | Shared files /usr/local/share/soot/ |
| `get-path`                 | `'project`     | Project folder                      |
| `find-file`                | `filename`     | Find file in system directories     |

**Examples:**

```scheme
(system "ls -la")                     ; Execute command
(get-environment-variable "PATH")     ; Get PATH env
(get-path 'cwd)                       ; → "/current/path"
(exit 0)                              ; Exit with code 0
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

## 📖 **Reader and LexTokens**

### **Reader Functions**

| Function                   | Arguments                    | Description                                 |
|----------------------------|------------------------------|---------------------------------------------|
| `set-macro-character`      | `pattern replacement/lambda` | Define reader macro                         |
| `remove-macro-character`   | `pattern`                    | Remove reader macro                         |
| `get-macro-character`      | `pattern`                    | Get reader macro definition                 |
| `read`                     | `reader`                     | Read one object from reader                 |
| `read-char`                | `reader`                     | Read one character from reader              |
| `peek-char`                | `reader`                     | Peek next character without consuming       |
| `read-delimited-list`      | `terminator reader`          | Read list until terminator                  |

### **LexToken Functions**

| Function         | Arguments      | Description                         |
|------------------|----------------|-------------------------------------|
| `make-lextoken`  | `:type :value` | Create lex token with metadata      |
| `lextoken-type`  | `lextoken`     | Get token type                      |
| `lextoken-value` | `lextoken`     | Get token value                     |
| `lextoken-info`  | `lextoken`     | Get source location (file line col) |

**Examples:**

```scheme
;; Reader macros
(set-macro-character "$" "label")
(read "$test")                 ; → (label test)

(set-macro-character #\[ 
  (lambda (r s)
    (let ((lis (read-delimited-list #\] r)))
      (apply 'vector lis))))

;; Lex tokens
(define token (make-lextoken :type 'identifier :value 'x))
(lextoken-type token)         ; → identifier
(lextoken-info token)         ; → ("repl" 1 5)
```

## 🔄 **Macro System**

| Function      | Arguments    | Description                      |
|---------------|--------------|----------------------------------|
| `macroexpand` | `expression` | Expand macros in expression      |

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
|------------|----------------|-----------------------|
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
|-----------------|--------------------------------------------------------------------------------------------------------------------------------------|
| Core            | `define`, `set!`, `lambda`, `macro`, `if`, `cond`, `let`, `let*`, `quote`, `quasiquote`                                              |
| Math            | `+`, `-`, `*`, `/`, `=`, `<`, `>`, `<=`, `>=`, `abs`, `max`, `min`, `expt`, `sqrt`, `ash`                                            |
| Lists           | `cons`, `car`, `cdr`, `list`, `length`, `append`, `apply`, `set-car!`, `set-cdr!`                                                    |
| Strings         | `string-append`, `string-length`, `string-ref`, `string-substr`,`string_starts_with?`, `string-ends-with?`, `string-split`           |
| Vectors         | `vector`, `vector-ref`, `vector-set!`, `vector-length`, `vector->list`                                                               |
| Hash Tables     | `make-hash-table`, `hash-table-set!`, `hash-table-ref`, `hash-table-try-ref`, `hash-table?`, `hash-table-length`, `hash-table->list` |
| Type Predicates | `null?`, `pair?`, `symbol?`, `number?`, `string?`, `char?`, `vector?`, `procedure?`, `boolean?`, `type?`, `reader?`, `lextoken?`     |
| Comparisons     | `eq?`, `eqv?`                                                                                                                        |
| Conversions     | `number->string`, `string->number`, `char->integer`, `integer->char`, `string->symbol`, `symbol->string`                             |
| File I/O        | `print`, `pprint`, `inspect`, `fmt`, `error`, `file-exists?`, `read-str`, `parse-str`, `load`, `read-file`                           |
| System          | `system`, `get-environment-variable`, `exit`, `get-path`, `find-file`                                                                |
| Time            | `time-seconds`, `time-milliseconds`, `time-microseconds`, `time-nanoseconds`                                                         |
| Reader          | `set-macro-character`, `remove-macro-character`, `get-macro-character`, `read`, `read-char`, `peek-char`, `read-delimited-list`      |
| LexTokens       | `make-lextoken`, `lextoken-type`, `lextoken-value`, `lextoken-info`                                                                  |
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

### `(deftype name (parent) (fields...))`

Defines a new structure or basic type. The C++ backend automatically calculates field offsets, alignment, and total size based on the target architecture (Z80).

**Example:**

```lisp
(deftype Sprite ()
  (x uint8)
  (y uint8)
  (data (pointer uint8)))

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
