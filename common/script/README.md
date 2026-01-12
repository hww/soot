# 📖 **SOOT Script Language - Quick Reference**

SOOT (Scriptable Object-Oriented Toolkit) - Scheme-like embedded scripting language for C++ applications.

## 📚 **Table of Contents**

- [📋 Argument Syntax](#argument-syntax)
- [🎯 Special Forms](#special-forms)
- [🔧 Built-in Functions](#built-in-functions)
- [🔢 Mathematics](#mathematics)
- [📝 Strings](#strings)
- [🔗 Lists and Pairs](#lists-and-pairs)
- [🗃️ Vectors](#vectors)
- [🔍 Type Predicates](#type-predicates)
- [⚖️ Comparisons](#comparisons)
- [🗄️ Hash Tables](#hash-tables)
- [💾 File I/O](#file-io)
- [🖥️ System](#system)
- [🔄 Type Conversions](#type-conversions)
- [🎲 Other Functions](#other-functions)

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

| Function                   | Arguments           | Description                   |
|----------------------------|---------------------|-------------------------------|
| `print` `pprint` `inspect` | `value`             | Print with different formats  |
| `fmt`                      | `#t format args...` | Formatted output              |
|                            | `#f format args...` | Formatt to string             |
| `error`                    | `message`           | Throw error                   |
| `read`                     | `string`            | Read from string              |
| `load-file`                | `filename`          | Load and execute file         |
| `read-file`                | `filename`          | Read file contents            |
| `try-load-file`            | `filename`          | Load file, return #f if fails |
| `file-exists?`             | `filename`          | Check if file exists          |
| `read-data-file`           | `filename`          | Read data file                |

**Examples:**

```scheme
(print "Hello")                    ; Print value
(load-file "script.sot")           ; Execute file
(file-exists? "data.txt")          ; → #t or #f
(fmt #t "Hello ~a" :arg1 "World")  ; Print formatted
```

## 🖥️ **System**

| Function                   | Arguments | Description           |
|----------------------------|-----------|-----------------------|
| `system`                   | `command` | Execute shell command |
| `get-environment-variable` | `name`    | Get env variable      |
| `current-directory`        |           | Get current directory |
| `exit`                     |           | Exit interpreter      |

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