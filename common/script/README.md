# 📖 SOOT Script Language - Quick Reference (Updated)

SOOT (Scriptable Object-Oriented Toolkit) - Scheme-like embedded scripting language for C++ applications.

## 📚 Table of Contents

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
- [Environment Functions](#environment-functions)
- [Error Handling](#error-handling)
- [Getter-Setter System](#getter-setter-system)
- [Iteration Functions](#iteration-functions)
- [Type System & Metaprogramming](#type-system--metaprogramming)

---

## 📋 Argument Syntax

### Basic Syntax

```scheme
; Required arguments
(lambda (x y z) (+ x y z))          ; x, y, z are required

; Variable number of arguments
(lambda args ...)                   ; all args collected in 'args'
(lambda (a b &rest rest) ...)      ; a, b required, rest in 'rest'

; Keyword arguments (must start with :)
(lambda (&key name age) ...)       ; optional keyword args
(lambda (&key (name "John")) ...) ; keyword with default value

; Optional arguments
(lambda (&optional x y) ...)       ; x, y optional
(lambda (&optional (x 10)) ...)    ; x optional with default 10
```

### Calling Functions

```scheme
(function arg1 arg2 :key1 value1 :key2 value2)
;                           ↑
; Keyword args must have colon prefix!
```

### Argument Order Rules

1. Required positional arguments first
2. `&optional` optional positional arguments
3. `&key` keyword arguments
4. `&rest` variable arguments (last)

---

## 🎯 Special Forms

Special forms don't evaluate their arguments automatically.

### Variable Definition

```scheme
(define x 10)                  ; Define variable
(define x 10 :env some-env)    ; Define in specific environment
(set! x 20)                    ; Modify existing variable
```

### Conditionals

```scheme
(cond 
  (condition1 expr1)
  (condition2 expr2)
  (else default-expr))

(or expr1 expr2 ...)           ; Short-circuit OR
(and expr1 expr2 ...)          ; Short-circuit AND
```

### Functions & Macros

```scheme
(lambda (args) body...)       ; Function definition
(macro (args) body...)        ; Macro definition
(let ((var1 val1) ...) body)  ; Local bindings
(let* ((var1 val1) ...) body) ; Sequential bindings
(let name ((var1 val1) ...) body) ; Named let
(while condition body...)     ; While loop
```

### Quoting

```scheme
(quote expr)                  ; or 'expr
(quasiquote expr)             ; or `expr with , and ,@
```

### Control Flow

```scheme
(begin expr1 expr2 ...)       ; Sequence of expressions
(top-level expr1 expr2 ...)   ; Top-level evaluation
```

### Error Handling

```scheme
(with-error-handler handler-fn body...)
```

### Other Special Forms

```scheme
(eval-args arg1 arg2 ...)     ; Evaluate arguments in place (for macros)
```

---

## 🔧 Built-in Functions

## 🔢 Mathematics

| Function          | Arguments          | Description      |
| ----------------- | ------------------ | ---------------- |
| `+`               | `number...`        | Sum of numbers   |
| `-`               | `number number...` | Subtraction      |
| `*`               | `number...`        | Multiplication   |
| `/`               | `number number`    | Division         |
| `=`               | `number number...` | Numeric equality |
| `<` `>` `<=` `>=` | `number number`    | Comparisons      |
| `abs`             | `number`           | Absolute value   |
| `max` `min`       | `number...`        | Maximum/Minimum  |
| `expt`            | `base exponent`    | Power            |
| `sqrt`            | `number`           | Square root      |
| `ash`             | `integer shift`    | Arithmetic shift |
| `floor`           | `number`           | Round down       |
| `ceiling`         | `number`           | Round up         |
| `round`           | `number`           | Round to nearest |
| `mod`             | `integer integer`  | Modulo           |
| `sin` `cos` `tan` | `number`           | Trigonometry     |
| `atan`            | `number [number]`  | Arctangent       |

### Bitwise Operations

| Function | Arguments          | Description      |
| -------- | ------------------ | ---------------- |
| `logand` | `integer...`       | Bitwise AND      |
| `logior` | `integer...`       | Bitwise OR       |
| `logxor` | `integer...`       | Bitwise XOR      |
| `lognot` | `integer`          | Bitwise NOT      |
| `lshift` | `integer shift`    | Left shift       |
| `rshift` | `integer shift`    | Right shift      |

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
(logand #b1100 #b1010) ; → #b1000
(logior #b1100 #b1010) ; → #b1110
```

## 📝 Strings

| Function              | Arguments          | Description              |
| --------------------- | ------------------ | ------------------------ |
| `string-append`       | `string...`        | Concatenate strings      |
| `string-length`       | `string`           | String length            |
| `string-ref`          | `string integer`   | Get character at index   |
| `string-substr`       | `string start end` | Substring (negative OK)  |
| `string-replace`      | `string from to`   | Replace all occurrences  |
| `string-prefix?`      | `string prefix`    | Check prefix             |
| `string-suffix?`      | `string suffix`    | Check suffix             |
| `string-contains?`    | `string substring` | Check contains           |
| `string-split`        | `string delim`     | Split into list          |
| `string-join`         | `list delim`       | Join list with delimiter |
| `string-to-lower`     | `string`           | Convert to lowercase     |
| `string-to-upper`     | `string`           | Convert to uppercase     |
| `string-titelize`     | `string`           | Title case               |
| `string-trim`         | `string`           | Trim whitespace          |
| `string-ltrim`        | `string`           | Trim left                |
| `string-rtrim`        | `string`           | Trim right               |
| `string-trim-idents`  | `string`           | Trim newline indents     |
| `string->symbol`      | `string`           | Convert to symbol        |
| `symbol->string`      | `symbol`           | Convert to string        |
| `string-for-each`     | `string lambda`    | Iterate over characters  |

**Examples:**

```scheme
(string-append "Hello" " " "World") ; → "Hello World"
(string-length "test")              ; → 4
(string-ref "abc" 1)                ; → #\b
(string-substr "hello" 1 4)         ; → "ell"
(string-substr "hello" -3 -1)       ; → "ll"
(string-split "a,b,c" ",")          ; → ("a" "b" "c")
(string-join '("a" "b" "c") "-")    ; → "a-b-c"
(string-prefix? "hello" "he")       ; → #t
```

## 🔗 Lists and Pairs

| Function                  | Arguments    | Description                      |
| ------------------------- | ------------ | -------------------------------- |
| `cons`                    | `item list`  | Create pair                      |
| `car`                     | `pair`       | First element                    |
| `cdr`                     | `pair`       | Rest of list                     |
| `set-car!` `set-cdr!`     | `pair value` | Modify pair                      |
| `list`                    | `item...`    | Create list                      |
| `length`                  | `list`       | List length                      |
| `append`                  | `list...`    | Concatenate lists                |
| `apply`                   | `func list`  | Apply function to list arguments |
| `getf`                    | `key plist`  | Get value from property list     |
| `assoc`                   | `key alist`  | Find pair in association list    |

**Examples:**

```scheme
(cons 1 '(2 3))          ; → (1 2 3)
(car '(a b c))           ; → a
(cdr '(a b c))           ; → (b c)
(list 1 2 3)             ; → (1 2 3)
(length '(a b c d))      ; → 4
(append '(1 2) '(3 4))   ; → (1 2 3 4)
(apply + '(1 2 3))       ; → 6
(getf 'b '(:a 1 :b 2))   ; → 2
(assoc 'b '((a . 1) (b . 2))) ; → (b . 2)
```

## 🗃️ Vectors (Arrays)

| Function          | Arguments            | Description            |
| ----------------- | -------------------- | ---------------------- |
| `make-array`      | `item...`            | Create array           |
| `array-ref`       | `array integer`      | Get element            |
| `array-set!`      | `array integer value`| Set element            |
| `array-length`    | `array`              | Array size             |
| `array->list`     | `array`              | Convert array to list  |
| `array-for-each`  | `array lambda`       | Iterate over elements  |

**Examples:**

```scheme
(make-array 1 2 3)               ; → #(1 2 3)
(array-ref #(a b c) 1)           ; → b
(array-set! #(1 2 3) 1 99)       ; → #(1 99 3)
(array-length #(a b c d))        ; → 4
(array->list #(1 2 3))           ; → (1 2 3)
```

## 🔍 Type Predicates

| Function      | Arguments         | Returns                |
| ------------- | ----------------- | ---------------------- |
| `null?`       | `value`           | #t if empty list       |
| `pair?`       | `value`           | #t if pair/list        |
| `type?`       | `value type-name` | Check specific type    |
| `keyword?`    | `value`           | #t if keyword          |
| `type-of`     | `value`           | Returns type as symbol |

**Examples:**

```scheme
(null? '())              ; → #t
(pair? '(1 2))           ; → #t
(type? 'integer 5)       ; → #t
(type? 'string "test")   ; → #t
(type-of "hello")        ; → string
(keyword? :foo)          ; → #t
```

## ⚖️ Comparisons

| Function | Arguments | Description     |
| -------- | --------- | --------------- |
| `eq?`    | `any any` | Object equality |

**Examples:**

```scheme
(eq? 'a 'a)              ; → #t
(eq? '(1 2) '(1 2))      ; → #f (different objects)
(eq? 5 5)                ; → #t
```

## 🗄️ Hash Tables

| Function                | Arguments         | Description              |
| ----------------------- | ----------------- | ------------------------ |
| `make-hash-table`       | `pairs... :size n :type t` | Create hash table |
| `hash-table-set!`       | `table key value` | Set value                |
| `hash-table-ref`        | `table key [default]` | Get value            |
| `hash-table-try-ref`    | `table key`       | Returns (success? value) |
| `hash-table-contains?`  | `table key`       | Check if key exists      |
| `hash-table-length`     | `table`           | Number of entries        |
| `hash-table->list`      | `table`           | Convert to list of pairs |
| `hash-table-for-each`   | `table lambda`    | Iterate over entries     |

**Examples:**

```scheme
(define ht (make-hash-table))
(hash-table-set! ht "name" "John")
(hash-table-ref ht "name")       ; → "John"
(hash-table-ref ht "age" 0)      ; → 0 (default)
(hash-table-try-ref ht "age")    ; → (#t 30) or (#f ())
(hash-table-contains? ht "name") ; → #t
(hash-table-length ht)           ; → 2
```

### Universal Table Access

| Function   | Arguments               | Description                    |
| ---------- | ----------------------- | ------------------------------ |
| `get-at`   | `target key [default]`  | Universal getter               |
| `set-at!`  | `target key value`      | Universal setter               |

## 💾 File I/O

| Function                   | Arguments                          | Description                  |
| -------------------------- | ---------------------------------- | ---------------------------- |
| `print`                    | `value`                            | Print value                  |
| `fmt`                      | `dest format args...`              | Formatted output             |
| `pfmt`                     | `value :width n`                   | Pretty-print to string       |
| `inspect`                  | `value`                            | Inspect object structure     |
| `error`                    | `message :ctx object`              | Throw error with context     |
| `file-exists?`             | `filename`                         | Check if file exists         |
| `read-str`                 | `filename`                         | Read file to string          |
| `parse-str`                | `string`                           | Parse string to code         |
| `load`                     | `filename`                         | Load and execute file        |
| `read-file`                | `filename`                         | Read file contents as code   |
| `read-text-file`           | `filename`                         | Read text file               |
| `write-text-file`          | `filename content`                 | Write text file              |
| `read-binary-file`         | `filename`                         | Read binary file             |
| `write-binary-file`        | `filename data`                    | Write binary file            |

**Examples:**

```scheme
(print "Hello")                    ; Print value
(load "script.sot")               ; Execute file
(fmt #t "Hello ~a" "World")       ; Print formatted
(fmt #f "Value: ~d" 42)           ; → "Value: 42"
(fmt #t "Error!" :color "red")    ; Colored output
(error "File not found" :ctx form) ; Throw error
```

**Colors:** `"red"`, `"green"`, `"yellow"`, `"blue"`, `"magenta"`, `"cyan"`, `"white"`, `"gray"`

**Logging:**

```scheme
(log 'info "Message ~a" value)
(log 'debug "Debug: ~a" data)
(log 'error "Error: ~a" msg)
```

## 🖥️ System

| Function                   | Arguments  | Description                         |
| -------------------------- | ---------- | ----------------------------------- |
| `system`                   | `command`  | Execute shell command               |
| `get-environment-variable` | `name`     | Get env variable                    |
| `exit`                     | `code`     | Exit interpreter                    |
| `get-path`                 | `'cwd`     | Current working dir                 |
| `get-path`                 | `'exe`     | Executable directory                |
| `get-path`                 | `'home`    | User home                           |
| `get-path`                 | `'config`  | User config directory               |
| `get-path`                 | `'cache`   | Cache directory                     |
| `get-path`                 | `'share`   | Shared files directory              |
| `get-path`                 | `'project` | Project folder                      |
| `find-file`                | `filename` | Find file in system directories     |

## 🔄 Type Conversions

| Function         | Arguments              | Description        |
| ---------------- | ---------------------- | ------------------ |
| `number->string` | `number :base n`       | Convert to string  |
| `string->number` | `string :base n`       | Parse number       |
| `char->integer`  | `char`                 | Character to ASCII |
| `integer->char`  | `integer`              | ASCII to character |

**Examples:**

```scheme
(number->string 42 :base 16)     ; → "2a"
(string->number "FF" :base 16)   ; → 255
(char->integer #\A)              ; → 65
(integer->char 66)               ; → #\B
```

## 🎲 Time Functions

| Function            | Arguments | Description                          |
| ------------------- | --------- | ------------------------------------ |
| `time-seconds`      |           | Unix timestamp in seconds            |
| `time-milliseconds` |           | Unix timestamp in milliseconds       |
| `time-microseconds` |           | Unix timestamp in microseconds       |
| `time-nanoseconds`  |           | Unix timestamp in nanoseconds        |

## 📖 Reader Functions

| Function                 | Arguments                    | Description                     |
| ------------------------ | ---------------------------- | ------------------------------- |
| `set-macro-character`    | `pattern replacement/lambda` | Define reader macro             |
| `remove-macro-character` | `pattern`                    | Remove reader macro             |
| `get-macro-character`    | `pattern`                    | Get reader macro definition     |
| `read`                   | `reader`                     | Read one object from reader     |
| `read-char`              | `reader`                     | Read one character              |
| `peek-char`              | `reader`                     | Peek without consuming          |
| `read-delimited-list`    | `terminator reader`          | Read list until terminator      |
| `source-info`            | `object`                     | Get source code information     |
| `get-context`            | `integer offset`             | Get stack frame expression      |

**Examples:**

```scheme
;; Reader macros
(set-macro-character "$" "label")
(read "$test")                 ; → (label test)

;; Lambda reader macro
(set-macro-character #\[ 
  (lambda (r s)
    (let ((lis (read-delimited-list #\] r)))
      (apply 'vector lis))))
```

### Source Information

```scheme
(source-info '(1))
; → (:file "repl" :line 0 :column 14 :text "(source-info '(1))")
```

### Stack Context

```scheme
(get-context 0)   ; Current frame
(get-context 1)   ; Parent frame
(get-context 2)   ; Grandparent frame
```

## 🔄 Macro System

| Function      | Arguments    | Description                 |
| ------------- | ------------ | --------------------------- |
| `macroexpand` | `expression` | Expand macros in expression |

**Examples:**

```scheme
(macroexpand '(my-or a b c))
; → (if a a (my-or b c))
```

## 🎲 Other Functions

| Function          | Arguments           | Description            |
| ----------------- | ------------------- | ---------------------- |
| `gensym`          |                     | Generate unique symbol |
| `eval`            | `expression`        | Evaluate expression    |
| `eval`            | `func args... :env e` | Apply function       |

**Examples:**

```scheme
(gensym)        ; → gensym0, gensym1, ...
(eval '(+ 1 2)) ; → 3
(eval (lambda (x) (* x 2)) 5) ; → 10
```

## 🌍 Environment Functions

| Function              | Arguments           | Description                    |
| --------------------- | ------------------- | ------------------------------ |
| `env`                 |                     | Get current environment        |
| `current-env`         |                     | Get current environment        |
| `defined?`            | `symbol`            | Check if symbol is defined     |
| `lookup`              | `symbol :env e`     | Look up symbol value           |
| `current-function`    | `'function`/`'asm`  | Get current function           |
| `env-name`            |                     | Get environment name           |
| `env-name!`           | `symbol`            | Set environment name           |

**Examples:**

```scheme
(defined? 'x)           ; → #t or #f
(lookup 'x)             ; → value or none
(env-name)              ; → symbol
```

## ⚠️ Error Handling

### Special Form

```scheme
(with-error-handler handler-lambda body...)
```

**Handler receives:** `(message error-form trace-list)`

**Handler returns:**
- `#f` or `null` - suppress error
- `#t` - re-throw error
- `(context . "message")` - add context and re-throw
- Other value - suppress and return value

**Example:**

```scheme
(with-error-handler
  (lambda (msg form trace)
    (fmt #t "Error: ~a\n" msg)
    #f)  ; Suppress error
  (error "Something went wrong"))
```

### Error Function

```scheme
(error "message" :ctx object)  ; Throw error with context
```

## 🔧 Getter-Setter System

| Function         | Arguments              | Description                    |
| ---------------- | ---------------------- | ------------------------------ |
| `defsetf`        | `getter setter`        | Register getter-setter pair    |
| `get-setter`     | `getter`               | Get setter for getter          |

**Example:**

```scheme
(defsetf 'car 'set-car!)
(defsetf 'hash-table-ref 'hash-table-set!)

;; Used by setf macro
(setf (car x) 10)  ; Expands to (set-car! x 10)
```

## 🔁 Iteration Functions

| Function               | Arguments           | Description                    |
| ---------------------- | ------------------- | ------------------------------ |
| `list-for-each`        | `list lambda`       | Iterate over list elements     |
| `list-for-each-pair`   | `list lambda`       | Iterate over key-value pairs   |
| `string-for-each`      | `string lambda`     | Iterate over characters        |
| `array-for-each`       | `array lambda`      | Iterate over array elements    |
| `hash-table-for-each`  | `table lambda`      | Iterate over hash table        |

**Lambda signatures:**
- 1 argument: `(item)` or `(char-code)`
- 2 arguments: `(index item)` or `(key value)`

**Examples:**

```scheme
(list-for-each '(1 2 3) (lambda (x) (print x)))
(list-for-each '(a b) (lambda (i x) (fmt #t "~d: ~a\n" i x)))
(hash-table-for-each ht (lambda (k v) (fmt #t "~a = ~a\n" k v)))
```

## 🔧 Function Metadata

| Function         | Arguments              | Description                    |
| ---------------- | ---------------------- | ------------------------------ |
| `function-name`  | `function`             | Get function name              |
| `function-name!` | `function name`        | Set function name              |

**Examples:**

```scheme
(function-name my-func)        ; → name or ""
(function-name! my-func 'new-name)
```

---

## 🚀 Quick Start Examples

### Hello World

```scheme
(print "Hello, World!")
```

### Function Definition

```scheme
(define (square x) (* x x))
(square 5)  ; → 25
```

### List Processing

```scheme
(define (map f lst)
  (if (null? lst)
      '()
      (cons (f (car lst)) (map f (cdr lst)))))

(map (lambda (x) (* x 2)) '(1 2 3 4))  ; → (2 4 6 8)
```

### Error Handling

```scheme
(with-error-handler
  (lambda (msg form trace)
    (fmt #t "Caught: ~a\n" msg)
    #f)
  (error "Test error"))
```

### Working with Hash Tables

```scheme
(define config (make-hash-table))
(hash-table-set! config "debug" #t)
(hash-table-set! config "level" 5)

(hash-table-for-each config
  (lambda (key value)
    (fmt #t "~a: ~a\n" key value)))
```

---

## ⚠️ Common Pitfalls

1. **Keywords need colon**: `:flag` not `flag` in function calls
2. **Lists vs. Arrays**: Use `()` for lists, `#(...)` for arrays
3. **Equality**: Use `eq?` for comparison
4. **Macro arguments**: Macros don't evaluate arguments automatically
5. **&rest placement**: Must be last in argument list

---

## 🔗 Useful Patterns

```scheme
;; Optional arguments with defaults
(lambda (&optional (verbose #f) (level 1))
  (if verbose (print "Level:" level)))

;; Keyword arguments with defaults
(lambda (&key (name "default") (age 0))
  (list name age))

;; Variable arguments
(lambda (&rest args)
  (apply + args))

;; Error handling with context
(if (file-exists? "config.sot")
    (load "config.sot")
    (error "Config not found" :ctx 'config.sot))
```

---

## 📖 Type Reference

| Type       | Literal        | Example               |
| ---------- | -------------- | --------------------- |
| Integer    | `123`          | `42`, `-5`, `0xFF`    |
| Float      | `1.23`         | `3.14`, `-2.5`        |
| Boolean    | `#t`, `#f`     | `#t`, `#f`            |
| String     | `"text"`       | `"Hello"`, `"test\n"` |
| Character  | `#\c`          | `#\a`, `#\newline`    |
| Symbol     | `'name`        | `'x`, `'my-var`       |
| Keyword    | `:name`        | `:key`, `:value`      |
| List       | `(a b c)`      | `'(1 2 3)`, `()`      |
| Array      | `#(a b c)`     | `#(1 2 3)`            |
| Pair       | `(a . b)`      | `(1 . 2)`             |
| Lambda     | `(lambda ...)` | Function object       |
| Macro      | `(macro ...)`  | Macro object          |
| Hash Table | `#{}`          | Hash table object     |
| Reader     |                | Text stream reader    |

---

## 🎯 REPL Commands

```bash
soot> (+ 1 2 3)        ; Evaluate expression
soot> quit             ; Exit REPL
soot> exit             ; Exit REPL
```

---

## 📋 Function Index

| Category        | Functions                                                                                                                            |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Special Forms   | `define`, `set!`, `quote`, `lambda`, `macro`, `cond`, `or`, `and`, `let`, `let*`, `while`, `quasiquote`, `with-error-handler`, `eval-args` |
| Math            | `+`, `-`, `*`, `/`, `=`, `<`, `>`, `<=`, `>=`, `abs`, `max`, `min`, `expt`, `sqrt`, `ash`, `floor`, `ceiling`, `round`, `mod`, `sin`, `cos`, `tan`, `atan` |
| Bitwise         | `logand`, `logior`, `logxor`, `lognot`, `lshift`, `rshift`                                                                           |
| Lists           | `cons`, `car`, `cdr`, `list`, `length`, `append`, `apply`, `set-car!`, `set-cdr!`, `getf`, `assoc`                                    |
| Strings         | `string-append`, `string-length`, `string-ref`, `string-substr`, `string-replace`, `string-prefix?`, `string-suffix?`, `string-contains?`, `string-split`, `string-join`, `string-to-lower`, `string-to-upper`, `string-trim`, `string-for-each` |
| Arrays          | `make-array`, `array-ref`, `array-set!`, `array-length`, `array->list`, `array-for-each`                                              |
| Hash Tables     | `make-hash-table`, `hash-table-set!`, `hash-table-ref`, `hash-table-try-ref`, `hash-table-contains?`, `hash-table-length`, `hash-table->list`, `hash-table-for-each` |
| Type Predicates | `null?`, `type?`, `keyword?`, `type-of`                                                                                               |
| Comparisons     | `eq?`                                                                                                                                 |
| Conversions     | `number->string`, `string->number`, `char->integer`, `integer->char`, `string->symbol`, `symbol->string`                              |
| File I/O        | `print`, `fmt`, `pfmt`, `inspect`, `error`, `file-exists?`, `read-str`, `parse-str`, `load`, `read-file`, `read-text-file`, `write-text-file`, `read-binary-file`, `write-binary-file` |
| System          | `system`, `get-environment-variable`, `exit`, `get-path`, `find-file`                                                                 |
| Time            | `time-seconds`, `time-milliseconds`, `time-microseconds`, `time-nanoseconds`                                                         |
| Reader          | `set-macro-character`, `remove-macro-character`, `get-macro-character`, `read`, `read-char`, `peek-char`, `read-delimited-list`, `source-info`, `get-context` |
| Macro System    | `macroexpand`                                                                                                                         |
| Environment     | `env`, `current-env`, `defined?`, `lookup`, `current-function`, `env-name`, `env-name!`                                              |
| Iteration       | `list-for-each`, `list-for-each-pair`, `string-for-each`, `array-for-each`, `hash-table-for-each`                                   |
| Table Access    | `get-at`, `set-at!`                                                                                                                   |
| Getter/Setter   | `defsetf`, `get-setter`                                                                                                               |
| Functions       | `function-name`, `function-name!`                                                                                                     |
| Other           | `gensym`, `eval`, `log`, `crc32`                                                                                                      |

---

**Version**: SOOT Core  
**Syntax**: Scheme-like with Common Lisp influences  
**License**: Project-specific

## 📖 Other Documentation

- 📖 [SOOT Common Library Documentation](common/script/README.LIB.md)

---

## 🛠 Type System & Metaprogramming

The interpreter features a robust type system integration inspired by OpenGOAL.

### `(defenum name (entries...))`

Registers an enumeration in the global type database.

```lisp
(defenum color
        :type uint32
        (red 0) 
        (green 1)
        (blue 2))
```

For bitfields:

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

Defines a new structure or basic type.

```lisp
(deftype sprite (basic)
  ((x int32)
   (y int32)
   (data (pointer uint8))))
```

### `(typespec definition)`

Validates a type description and returns a canonical S-expression.

```lisp
(typespec (pointer uint32))  ; → (pointer uint32)
```

### `(type-info spec [property])`

Queries the C++ TypeSystem for metadata.

```lisp
(type-info '(pointer uint8) 'size)  ; → 2
```

### `(type-list)`

Returns a flat list of all registered type names.

---

## 📝 Summary of Changes

This documentation was updated to reflect the current implementation:

**Removed functions (not in current code):**
- `if` special form (use `cond` instead)
- `pprint` (use `pfmt` instead)
- `eqv?` (use `eq?` instead)
- Many type predicates (`pair?`, `symbol?`, `number?`, etc.) - use `type?` instead
- `vector` functions (renamed to `array`)
- `declare-type`, `declare`, `rlet`, `rlet-ref`, `make-asm-regs`, `declarations`, `set-method` (not in Interpreter)

**Added functions:**
- `with-error-handler` special form
- `eval-args` special form
- Environment functions: `env`, `current-env`, `lookup`, `current-function`, `env-name`, `env-name!`
- Iteration functions: `list-for-each`, `list-for-each-pair`, `string-for-each`, `array-for-each`, `hash-table-for-each`
- Table access: `get-at`, `set-at!`
- Getter/Setter: `defsetf`, `get-setter`
- Function metadata: `function-name`, `function-name!`
- Math: `floor`, `ceiling`, `round`, `mod`, `sin`, `cos`, `tan`, `atan`, `crc32`
- String: `string-replace`, `string-prefix?`, `string-suffix?`, `string-contains?`, `string-split`, `string-join`, `string-trim`, `string-trim-idents`
- Logging: `log`
- Binary I/O: `read-binary-file`, `write-binary-file`, `read-text-file`, `write-text-file`

**Renamed:**
- `vector` → `array`
- `vector-ref` → `array-ref`
- `vector-set!` → `array-set!`
- `vector-length` → `array-length`
- `vector->list` → `array->list`
- `make-vector` → `make-array`