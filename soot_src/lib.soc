;; lib.soc
;; ============================================================================
;; SOOTC COMPILER STANDARD LIBRARY
;; ============================================================================
;; This file is COMPILED by the compiler, not interpreted.
;; Its side effects:
;;   - defmacro registers macros in *soot-env* (via expansion into
;;     define ... macro ...)
;;   - deftype / defenum register types in m_ts
;;   - defconstant registers constants in the compiler environment
;;   - seval executes SOOT code immediately, during compilation of lib.soc
;;   - asm-file loads additional .soc / .gc files
;;
;; Anything that only needs to run while expanding macros belongs in lib.sot.
;; This file defines the SYNTAX of the language.
;;
;; IMPORTANT: the macros defined here live in *soot-env*, the
;; compiler's own environment. try_getting_macro_from_soot looks ONLY
;; in *soot-env*, so these macros are visible to the compiler. They
;; must NOT collide with names that NodeBuilder registers in
;; m_form_table, because if a macro and a built-in form share a name,
;; the macro wins and the built-in form is unreachable.
;;
;; Reserved names (do NOT define macros for these here):
;;   define, define-export, lambda, function, begin,
;;   if, cond, while, let, set!, new, ->,
;;   +, -, *, /, %, >, <, >=, <=, ==, !=,
;;   abs, neg, not, lognot,
;;   defmacro, deftype, defenum, defmethod, seval,
;;   update-macro-metadata, defconstant, define-extern, define-native
;; ============================================================================

;; ----------------------------------------------------------------------------
;; Core syntax macros
;; ----------------------------------------------------------------------------

(defmacro defun (name bindings &rest body)
  (let ((docstring ""))
    (when (and (> (length body) 1) (string? (first body)))
      (set! docstring (first body))
      (set! body (cdr body)))
    `(define ,name ,docstring (lambda :name ,name ,bindings ,@body))))

(defmacro defmacro (name args &rest body)
  `(define ,name (macro ,args ,@body)))

(defmacro defsmacro (name args &rest body)
  `(seval (defsmacro ,name ,args ,@body)))

(defmacro desfun (name args &rest body)
  `(seval (desfun ,name ,args ,@body)))

;; NOTE: `let` is a built-in form (LetNode). Do not define a macro for
;; it here. The compiler lowers (let ((x 1) (y 2)) body) directly into
;; a LetNode.

;; NOTE: `let*` is not a built-in form. We expand it into nested `let`
;; forms, which the compiler handles natively.
(defmacro let* (bindings &rest body)
  (if (null? bindings)
    `(begin ,@body)
    `(let (,(car bindings))
        (let* ,(cdr bindings) ,@body))))

;; `when` and `unless` expand into the built-in `if` and `begin`.
(defmacro when (condition &rest body)
  `(if ,condition (begin ,@body)))

(defmacro unless (condition &rest body)
  `(if (not ,condition) (begin ,@body)))

;; NOTE: `if` is a built-in form (IfNode). Do not define a macro for it.
;; NOTE: `not` is a built-in unary operation (UnaryNode::NOT). Do not
;; define a macro for it.
;; NOTE: `cond` is a built-in form (build_cond). Do not define a macro
;; for it.

(defmacro aif (condition true &rest false)
  `(let ((it ,condition))
     (if it ,true ,@false)))

(defmacro awhen (condition &rest body)
  `(aif ,condition (begin ,@body) #f))

;; ----------------------------------------------------------------------------
;; Loops
;; ----------------------------------------------------------------------------
;;
;; `while` is a BUILT-IN FORM (WhileNode). Do not define a macro for it.
;; WhileNode accepts an optional :label keyword, so the dependent
;; macros below can forward it.

(defmacro until (test &rest body)
  `(while (not ,test)
     ,@body))

(defmacro dotimes (var &key label &rest body)
  (let ((limit (second var))
        (name  (first var)))
    (if label
        `(let ((,name 0))
           (while (< ,name ,limit) :label ,label
             ,@body
             (set! ,name (+ ,name 1))))
        `(let ((,name 0))
           (while (< ,name ,limit)
             ,@body
             (set! ,name (+ ,name 1)))))))

(defmacro dolist (bindings &key label &rest body)
  (let ((name (car bindings))
        (lst  (cadr bindings)))
    (if label
        `(let ((,name ,lst))
           (while (not (null? ,name)) :label ,label
             ,@body
             (set! ,name (cdr ,name))))
        `(let ((,name ,lst))
           (while (not (null? ,name))
             ,@body
             (set! ,name (cdr ,name)))))))

(defmacro loop (&key label &rest body)
  (if label
      `(while #t :label ,label
         ,@body)
      `(while #t
         ,@body)))

(defmacro countdown (var &key label &rest body)
  (let ((name (first var))
        (init (second var)))
    (if label
        `(let ((,name ,init))
           (while (!= ,name 0) :label ,label
             (set! ,name (- ,name 1))
             ,@body))
        `(let ((,name ,init))
           (while (!= ,name 0)
             (set! ,name (- ,name 1))
             ,@body)))))

;; ----------------------------------------------------------------------------
;; Case
;; ----------------------------------------------------------------------------
;;
;; `case` expands into a `cond` chain using `comp` for the equality
;; test. The test defaults to `=`, which the compiler lowers to a
;; CompareNode; `string=` is a native function used for string
;; comparison.

(defmacro case (switch &key (comp =) &rest cases)
  (let ((switch-name (gensym)))
    `(let ((,switch-name ,switch))
       (cond
         ,@(map (lambda (clause)
                  (let ((head (first clause))
                        (body (cdr clause)))
                    (if (and (symbol? head) (eq? head 'else))
                      `(else ,@body)
                      `((or ,@(map (lambda (v) `(,comp ,switch-name ,v))
                                   head))
                        ,@body))))
                cases)))))

;; ----------------------------------------------------------------------------
;; Pairs and lists
;; ----------------------------------------------------------------------------

(defmacro cons (a b)
  `(new 'global 'pair ,a ,b))

(defmacro list (&rest args)
  (if (null? args)
    (quote '())
    `(cons ,(car args) (list ,@(cdr args)))))

(defmacro null? (arg)
  `(eq? ,arg '()))

(defmacro caar (arg)   `(car (car ,arg)))
(defmacro cadr (arg)   `(car (cdr ,arg)))
(defmacro cddr (arg)   `(cdr (cdr ,arg)))
(defmacro caddr (arg)  `(car (cdr (cdr ,arg))))
(defmacro cadddr (arg) `(car (cdr (cdr (cdr ,arg)))))

(defmacro cons! (lst val)
  `(set! ,lst (cons ,val ,lst)))

(defmacro swap! (a b)
  `(let ((temp ,a))
     (set! ,a ,b)
     (set! ,b temp)))

;; ----------------------------------------------------------------------------
;; Math macros
;; ----------------------------------------------------------------------------

(defmacro 1+ (var) `(+ ,var 1))
(defmacro 1- (var) `(+ ,var -1))
(defmacro inc (val) `(1+ ,val))
(defmacro dec (val) `(1- ,val))
(defmacro +! (place &rest args) `(set! ,place (+ ,place ,@args)))
(defmacro -! (place amount)     `(set! ,place (- ,place ,amount)))
(defmacro *! (place amount)     `(set! ,place (* ,place ,amount)))
(defmacro /! (place amount)     `(set! ,place (/ ,place ,amount)))
(defmacro 1+! (place) `(+! ,place 1))
(defmacro 1-! (place) `(-! ,place 1))
(defmacro inc! (place) `(+! ,place 1))
(defmacro dec! (place) `(-! ,place 1))
(defmacro zero? (thing)    `(eq? ,thing 0))
(defmacro nonzero? (thing) `(neq? ,thing 0))
(defmacro not! (var)  `(set! ,var (not ,var)))
(defmacro true! (var) `(set! ,var #t))
(defmacro false! (var) `(set! ,var #f))
(defmacro max! (val maxval) `(set! ,val (max ,val ,maxval)))
(defmacro min! (val minval) `(set! ,val (min ,val ,minval)))
(defmacro fmax! (val maxval) `(set! ,val (fmax ,val ,maxval)))
(defmacro fmin! (val minval) `(set! ,val (fmin ,val ,minval)))
(defmacro minmax (val minval maxval)  `(max (min ,val ,maxval) ,minval))
(defmacro fminmax (val minval maxval) `(fmax (fmin ,val ,maxval) ,minval))
(defmacro minmax! (val minval maxval)  `(set! ,val (max (min ,val ,maxval) ,minval)))
(defmacro fminmax! (val minval maxval) `(set! ,val (fmax (fmin ,val ,maxval) ,minval)))
(defmacro maxmin (val minval maxval)  `(min (max ,val ,maxval) ,minval))
(defmacro fmaxmin (val minval maxval) `(fmin (fmax ,val ,maxval) ,minval))

(defmacro xor (a b)
  (with-gensyms (a-temp b-temp)
    `(let ((,a-temp ,a) (,b-temp ,b))
        (or (and ,a-temp (not ,b-temp))
            (and (not ,a-temp) ,b-temp)))))

(defmacro logior! (place amount) `(set! ,place (logior ,place ,amount)))
(defmacro logxor! (place amount) `(set! ,place (logxor ,place ,amount)))
(defmacro logand! (place amount) `(set! ,place (logand ,place ,amount)))
(defmacro logclear (a b)  `(logand ,a (lognot ,b)))
(defmacro logclear! (a b) `(set! ,a (logand ,a (lognot ,b))))
(defmacro logtest? (a b)  `(nonzero? (logand ,a ,b)))
(defmacro logtesta? (a b) `(= (logand ,b ,a) ,b))

;; ----------------------------------------------------------------------------
;; Bit macros
;; ----------------------------------------------------------------------------

(defmacro align-n (val n)
  `(logand (- ,n) (+ (the-as int ,val) (- ,n 1))))

(defmacro align16 (val) `(align-n ,val 16))
(defmacro align64 (val) `(align-n ,val 64))

;; ----------------------------------------------------------------------------
;; Compiler control macros
;; ----------------------------------------------------------------------------

(defmacro m (file)
  "Make: compile a file fully and save the result."
  `(asm-file ,file :color :write))

(defmacro md (file &rest path)
  "Make Debug: make + print disassembly for a file."
  (if (null? path)
      `(asm-file ,file :color :write :disassemble)
      `(asm-file ,file :color :write :disassemble ,(first path))))

(defmacro mda (file &rest path)
  "Make Debug Asm Only."
  (if (null? path)
      `(asm-file ,file :color :write :disassemble :disasm-code-only)
      `(asm-file ,file :color :write :disassemble ,(first path) :disasm-code-only)))

(defmacro ml (file)
  "Make Load: make and load the file through the listener."
  `(asm-file ,file :color :load :write))

(defmacro import (file-name)
  `(asm-file ,file-name :no-code))

(defmacro e ()
  "Exit the compiler."
  `(:exit))

(defmacro dbc ()
  "Put the compiler into debug mode."
  `(begin
     (set-config! print-ir #t)
     (set-config! print-regalloc #t)))

(defmacro mi ()
  "Make ISO."
  `(make-group "iso"))

(defmacro mkr ()
  "Make kernel."
  `(make-group "kernel"))

(defmacro mng ()
  "Make engine."
  `(make-group "engine"))

;; ----------------------------------------------------------------------------
;; Conditional compilation
;; ----------------------------------------------------------------------------

(defmacro #when (clause &rest body)
  `(#cond (,clause ,@body)))

(defmacro #unless (clause &rest body)
  `(#cond ((not ,clause) ,@body)))

(defmacro #if (clause true false)
  `(#cond (,clause ,true) (#t ,false)))

;; ----------------------------------------------------------------------------
;; Define variants
;; ----------------------------------------------------------------------------

(defmacro defun-recursive (name return-type bindings &rest body)
  `(begin
     (define-extern ,name
         (function ,@(apply (lambda (x)
                              (if (pair? x) (second x) 'object))
                            bindings)
                   ,return-type))
     (defun ,name ,bindings ,@body)))

(defmacro defun-extern (function-name &rest type-info)
  `(define-extern ,function-name (function ,@type-info)))

(defmacro define-once (name value)
  `(begin
     (if (or (not ,name) (zero? ,name))
         (set! ,name ,value))))

(defmacro define-perm (name type value)
  `(begin
     (define-extern ,name ,type)
     (define-once ,name ,value)))

;; ----------------------------------------------------------------------------
;; Enum support
;; ----------------------------------------------------------------------------

(defmacro enum->string (enum input)
  `(case ,input
      ,@(apply (lambda (x) `(((,enum ,(car x))) ,(symbol->string (car x))))
               (reverse (get-enum-vals enum)))
      (else "*unknown*")))

(defmacro string->enum (enum input default)
  `(case-str ,input
      ,@(apply (lambda (x) `((,(symbol->string (car x))) (,enum ,(car x))))
               (get-enum-vals enum))
      (else ,default)))

;; ----------------------------------------------------------------------------
;; Structs
;; ----------------------------------------------------------------------------

(defmacro object-new (allocation type-to-make &rest sz)
  (if (null? sz)
      `(the (current-method-type)
            ((method-of-type object new) ,allocation ,type-to-make
             (the int (-> ,type-to-make size))))
      `(the (current-method-type)
            ((method-of-type object new) ,allocation ,type-to-make ,@sz))))

;; ----------------------------------------------------------------------------
;; Float macros
;; ----------------------------------------------------------------------------

(defmacro fabs (x)
  `(if (< (the float ,x) 0)
       (- (the float ,x))
       (the float ,x)))

(defmacro sqrtf (x) `(sqrtf-no-fabs (fabs ,x)))

(defmacro basically-zero? (val)
  `(<= (fabs (- ,val 0.0)) 0.000000000000000001))

(defmacro basically-not-zero? (val)
  `(> (fabs (- ,val 0.0)) 0.000000000000000001))

(defmacro fequal-epsilon? (a b epsilon)
  `(< (fabs (- ,a ,b)) ,epsilon))

;; ----------------------------------------------------------------------------
;; Enum declarations
;; ----------------------------------------------------------------------------

(defenum game-text-version
  (jak1-v1 10)
  (jak1-v2 11)
  (jak2 20)
  (jak3 30)
  (jakx 40))

;; ----------------------------------------------------------------------------
;; Constants
;; ----------------------------------------------------------------------------

(defconstant *default-territory* 0)

;; ----------------------------------------------------------------------------
;; Library extensions
;; ----------------------------------------------------------------------------

(asm-file "soot_src/common/lib/vf-macros.soc")

;; ----------------------------------------------------------------------------
;; Native
;; ----------------------------------------------------------------------------

(define-native _print   (function _varargs_ object))
(define-native _println (function _varargs_ object))
(define-native _add     (function int int object))
(define-native _sub     (function int int object))
(define-native _mul     (function int int object))
(define-native _div     (function int int object))
(define-native _abs     (function int object))
(define-native _sqrt    (function float object))
(define-native _to_int  (function object object))
(define-native _to_float (function object object))

;; ----------------------------------------------------------------------------
;; Native types
;; ----------------------------------------------------------------------------

(deftype vector (structure)
  ((x    float   :offset 0)
   (y    float   :offset 4)
   (z    float   :offset 8)
   (w    float   :offset 12))
  (:methods
    (len (vector) float)))

(defmethod len ((this vector))
  (+ (-> this x) (-> this y) (-> this z) (-> this w)))

(deftype cvector (structure)
  ((x    float   :offset 0)
   (y    float   :offset 4)
   (z    float   :offset 8)
   (w    float   :offset 12))
  (:methods
    (new (symbol float float float float) _type_)
    (len (cvector) float)))

(defmethod new cvector ((allocation symbol) (x float) (y float) (z float) (w float))
  (set! (-> this x) x)
  (set! (-> this y) y)
  (set! (-> this z) z)
  (set! (-> this w) w)
  this)

(defmethod len ((this cvector))
  (+ (-> this x) (-> this y) (-> this z) (-> this w)))

;; ----------------------------------------------------------------------------
;; Load marker
;; ----------------------------------------------------------------------------
;(define *soc-lib-loaded* #t)