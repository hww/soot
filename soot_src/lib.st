;; lib.sot
;; =============================================================================
;; SOOT INTERPRETER STANDARD LIBRARY
;; =============================================================================
;; This file is INTERPRETED, not compiled.
;; It contains only utilities and macros that run while expanding macros,
;; i.e. during the compilation of user code.
;;
;; Anything that defines the SYNTAX of the language (defun, defmacro, let,
;; while, case, if, when, ...) lives in lib.soc, because those are compiled
;; forms. Here we keep only what the interpreter itself needs.
;;
;; IMPORTANT: the macros defined here live in *global-env*, the
;; interpreter's own environment. The compiler does NOT see them:
;; Compiler::try_getting_macro_from_soot looks up names ONLY in
;; *soot-env*, without walking up to *global-env*. This is what keeps
;; the interpreter's `if`/`when`/`not`/`==`/`!=` from shadowing the
;; compiler's built-in forms.
;;
;; Mirrors the split in OpenGOAL: goos-lib.gs → *global-env*,
;; goal-lib.gc → *goal-env*, and the compiler only consults the latter.
;; =============================================================================

;; ===========================================================================
;; Metaprogramming
;; ===========================================================================

(define defsmacro (macro (name args &rest body)
		    `(define ,name (macro ,args ,@body))))

(defsmacro desfun (name args &rest body)
  `(define ,name (lambda ,args ,@body)))

(defsmacro defparameter (name &rest args)
  (define value (car args))
  `(define ,name ,value))

(defsmacro defvar (name &rest args)
  (define value (car args))
  `(if (not (defined? ',name))
       (define ,name ,value)
       null))

(defsmacro declare-extern (name type)
  `(declare-external ',name ',type))

;; soot macro to let us define goal macros from goos:
(defsmacro defgmacro (name args &rest body)
  `(define :env *soot-env* ,name (macro ,args ,@body))
  )

;; ===========================================================================
;; Interpreter control-flow macros
;; ===========================================================================
;;
;; These are the interpreter's OWN versions of if/when/not/unless/etc.
;; They are used when the interpreter expands a macro whose body
;; contains control-flow forms — for example, a lib.soc macro that has
;; `if` inside its body.
;;
;; The compiler does NOT see them, because try_getting_macro_from_soot
;; only consults *soot-env*. When the compiler encounters `if` in
;; compiled code, it goes to m_form_table and emits an IfNode.
;;
;; This is the same design OpenGOAL uses: goos-lib.gs defines if/when/
;; not/unless/etc. in *global-env*, while goal-lib.gc relies on the
;; compiler's built-in forms (compile_if, compile_when, ...). The two
;; coexist because they live in different environments.

(defsmacro if (clause true false)
  `(cond (,clause ,true) (#t ,false)))

(defsmacro when (clause &rest body)
  `(if ,clause (begin ,@body) #f))

(defsmacro not (x)
  `(if ,x #f #t))

(defsmacro unless (clause &rest body)
  `(if (not ,clause) (begin ,@body) #f))

(defsmacro aif (condition true false)
  "Anaphoric if, similar to Common Lisp"
  `(let ((it ,condition))
      (if it ,true ,false)))

;; ===========================================================================
;; Predicates (interpreter-side)
;; ===========================================================================

(defsmacro none?     (x) `(type? 'none ,x))
(defsmacro pair?     (x) `(type? 'pair ,x))
(defsmacro string?   (x) `(type? 'string ,x))
(defsmacro char?     (x) `(type? 'char ,x))
(defsmacro symbol?   (x) `(type? 'symbol ,x))
(defsmacro float?    (x) `(type? 'float ,x))
(defsmacro int?      (x) `(type? 'int ,x))
(defsmacro number?   (x) `(or (float? ,x) (int? ,x)))
(defsmacro bool?     (x) `(or (eq? #t ,x) (eq? #f ,x)))
(defsmacro function? (x) `(type? 'function ,x))
(defsmacro macro?    (x) `(type? 'macro ,x))
(defsmacro native-obj? (x) `(type? 'native-obj ,x))
(defsmacro array?    (x) `(type? 'array ,x))
(defsmacro hash-table? (x) `(type? 'hash-table ,x))
(defsmacro type-type? (x) `(type? 'type ,x))
(defsmacro type-spec? (x) `(type? 'type-spec ,x))

(desfun list? (x)
  (cond
    ((null? x) #t)
    ((pair? x) (list? (cdr x)))
    (#t #f)))

(defsmacro notnull? (x) `(not (null? ,x)))

;; ===========================================================================
;; Interpreter-side numeric / comparison macros
;; ===========================================================================
;;
;; `==`, `!=`, `neq?`, `between` are the interpreter's own macros. They
;; expand into interpreter calls (eq?, =, not) which the interpreter
;; knows how to evaluate. The compiler never sees them; when the
;; compiler encounters `==` in compiled code, it goes to m_form_table
;; and emits a CompareNode.

(defsmacro neq? (a b) `(not (eq? ,a ,b)))
(defsmacro !=   (a b) `(not (= ,a ,b)))
(defsmacro ==   (a b) `(eq? ,a ,b))
(defsmacro between (val min max)
  `(and (<= ,min ,val) (<= ,val ,max)))

;; ===========================================================================
;; Constants
;; ===========================================================================

(define pi 3.141592653589793)

;; ===========================================================================
;; Hash Table
;; ===========================================================================

(desfun gethash (key table &optional defval)
  "CL style hash table access"
  (if (none? defval)
      (hash-table-ref table key)
      (hash-table-ref table key defval)))

(desfun sethash (key table value)
  (hash-table-set! table key value))

(defsetf 'gethash 'sethash)

(desfun hash-table->keys (ht)
  (mapcar (lambda (pair) (car pair))
          (hash-table->list ht)))

(desfun hash-table->values (ht)
  (mapcar (lambda (pair) (cdr pair))
          (hash-table->list ht)))

(desfun hash-table-keys (ht)
  (let ((keys '()))
    (hash-table-for-each ht
      (lambda (key value)
        (set! keys (cons key keys))))
    keys))

(desfun hash-table-values (ht)
  (let ((vals '()))
    (hash-table-for-each ht
      (lambda (key value)
        (set! vals (cons value vals))))
    vals))

;; ===========================================================================
;; Interpreter-side case / case-type
;; ===========================================================================
;;
;; These are the interpreter's own case forms. lib.soc has its own
;; `case` that expands into a compiler-side cond. The two do not
;; conflict because they live in different environments and are used
;; by different consumers.

(defsmacro case (expr &rest branches)
  (let ((val-sym (gensym)))
    `(let ((,val-sym ,expr))
       (cond
         ,@(map (lambda (branch)
                  (let ((head (car branch))
                        (body (cdr branch)))
                    (if (eq? head 'else)
                        `(#t ,@body)
                        (if (list? head)
                            `((member ,val-sym ',head) ,@body)
                            `((eq? ,val-sym ',head) ,@body)))))
                branches)))))

(defsmacro case-type (obj &rest branches)
  (let ((type-sym (gensym)))
    `(let ((,type-sym (type-of ,obj)))
       (cond
         ,@(map (lambda (branch)
                  (let ((head (car branch))
                        (body (cdr branch)))
                    (if (eq? head 'else)
                        `(#t ,@body)
                        `((eq? ,type-sym ',head) ,@body))))
                branches)))))

;; ===========================================================================
;; Loops and iterators
;; ===========================================================================

(desfun for-each (container proc)
  (cond
   ((null? container)   null)
   ((list? container)   (list-for-each container proc))
   ((array? container)  (array-for-each container proc))
   ((string? container) (string-for-each container proc))
   ((hash-table? container) (hash-table-for-each container proc))
   (#t (error (fmt #f "for-each: unsupported container type {}" container)))))

;; ===========================================================================
;; MAP-style iterators
;; ===========================================================================

(desfun map (fun x)
  (if (null? x)
      '()
      (cons (fun (car x))
            (map fun (cdr x)))))

(desfun map2 (fun1 fun2 lst1 lst2)
  (if (or (null? lst1)
          (null? lst2)
          (not (= (length lst1) (length lst2))))
      '()
      (cons (fun1 (car lst1))
            (cons (fun2 (car lst2))
                  (map2 fun1 fun2 (cdr lst1) (cdr lst2))))))

(desfun flat-map (proc lis)
  (if (null? lis)
      '()
      (append (proc (car lis))
              (flat-map proc (cdr lis)))))

(desfun mapcar (func lst)
  (if (null? lst)
      null
      (cons (func (car lst))
            (mapcar func (cdr lst)))))

(desfun mapcar2 (func lst1 lst2)
  (if (or (null? lst1) (null? lst2))
      null
      (cons (func (car lst1) (car lst2))
            (mapcar2 func (cdr lst1) (cdr lst2)))))

(desfun mapcar3 (func lst1 lst2 lst3)
  (if (or (null? lst1) (null? lst2) (null? lst3))
      null
      (cons (func (car lst1) (car lst2) (car lst3))
            (mapcar3 func (cdr lst1) (cdr lst2) (cdr lst3)))))

(desfun any (pred lst)
  (cond ((null? lst) #f)
        ((pred (car lst)) #t)
        (else (any pred (cdr lst)))))

;; ===========================================================================
;; GENSYM
;; ===========================================================================

(defsmacro with-gensyms (names &rest body)
  `(let ,(map (lambda (x) `(,x (gensym))) names)
     ,@body))

;; ===========================================================================
;; List utilities
;; ===========================================================================

(desfun first (x)  (car x))
(desfun rest (x)   (cdr x))
(desfun second (x) (car (cdr x)))
(desfun third (x)  (car (cddr x)))

(desfun nth (n lst)
  (if (eq? n 0)
      (car lst)
      (nth (- n 1) (cdr lst))))

(desfun caar   (x) (car (car x)))
(desfun cadr   (x) (car (cdr x)))
(desfun cdar   (x) (cdr (car x)))
(desfun cddr   (x) (cdr (cdr x)))
(desfun caaar  (x) (car (car (car x))))
(desfun caadr  (x) (car (car (cdr x))))
(desfun cadar  (x) (car (cdr (car x))))
(desfun caddr  (x) (car (cdr (cdr x))))
(desfun cdddr  (x) (cdr (cdr (cdr x))))
(desfun cadddr (x) (car (cdr (cdr (cdr x)))))

(desfun make-list (len val)
  (if (<= len 0)
      '()
      (cons val (make-list (- len 1) val))))

(desfun copy-list (lis)
  (if (null? lis)
      '()
      (cons (car lis) (copy-list (cdr lis)))))

(desfun append (l1 l2)
  (if (null? l1)
      l2
      (cons (car l1) (append (cdr l1) l2))))

(desfun flatten (lis)
  (cond ((null? lis) null)
        ((pair? lis) (append (flatten (car lis)) (flatten (cdr lis))))
        (#t (list lis))))

(desfun reverse (lst)
  (let ((result '()) (iter lst))
    (while (not (null? iter))
      (set! result (cons (car iter) result))
      (set! iter (cdr iter)))
    result))

(desfun nreverse (lis)
  (let ((prev '())
        (curr lis)
        (next '()))
    (while (not (null? curr))
      (set! next (cdr curr))
      (set-cdr! curr prev)
      (set! prev curr)
      (set! curr next))
    prev))

(desfun filter (pred lst)
  (cond ((null? lst) '())
        ((pred (car lst))
         (cons (car lst) (filter pred (cdr lst))))
        (else (filter pred (cdr lst)))))

(desfun find-if (pred lst)
  (cond
    ((null? lst) null)
    ((pred (car lst)) (car lst))
    (else (find-if pred (cdr lst)))))

(desfun remove (item lst)
  (filter (lambda (x) (not (eq? x item))) lst))

(desfun remove-if (pred lst)
  (filter (lambda (x) (not (pred x))) lst))

(desfun member (x lst)
  (if (null? lst)
      #f
      (if (eq? (car lst) x)
          lst
          (member x (cdr lst)))))

(desfun assoc (x alist)
  (if (null? alist)
      #f
      (if (eq? (caar alist) x)
          (car alist)
          (assoc x (cdr alist)))))

(defsmacro push! (lst x)
  (cond
    ((symbol? lst)
     `(set! ,lst (cons ,x ,lst)))
    ((and (list? lst) (eq? (car lst) 'quote))
     `(set! ,(cadr lst) (cons ,x ,(cadr lst))))
    ((list? lst)
     (let ((getter (car lst))
           (args (cdr lst))
           (setter (get-setter (car lst))))
       (if setter
           `(,setter ,@args (cons ,x ,lst))
           (error (fmt #f "No setter registered for: {} in form {}" getter lst)))))
    (#t (error (fmt #f "Unsupported place for push!: {}" lst)))))

(defsmacro pop! (lst)
  (with-gensyms (tmp)
    `(let ((,tmp (car ,lst)))
       (set! ,lst (cdr ,lst))
       ,tmp)))

(desfun substitute (new old lst)
  (cond ((null? lst) '())
        ((eq? (car lst) old)
         (cons new (substitute new old (cdr lst))))
        (#t (cons (car lst) (substitute new old (cdr lst))))))

(desfun recsubstitute (dest source lis)
   (cond
      ((eq? lis source) dest)
      ((list? lis)
         (cond
            ((null? lis) lis)
            (#t (mapcar (lambda (lis) (recsubstitute dest source lis)) lis))))
      (#t lis)))

(desfun nsubstitute (new old lst)
  (let ((current lst))
    (while (not (null? current))
      (if (eq? (car current) old)
          (set-car! current new))
      (set! current (cdr current)))
    lst))

;; ===========================================================================
;; Mutation and arithmetic
;; ===========================================================================

(defsmacro 1+  (x) `(+ ,x 1))
(defsmacro 1-  (x) `(- ,x 1))
(defsmacro inc! (x) `(set! ,x (1+ ,x)))
(defsmacro dec! (x) `(set! ,x (1- ,x)))
(defsmacro +! (place arg) `(set! ,place (+ ,place ,arg)))
(defsmacro -! (place arg) `(set! ,place (- ,place ,arg)))

(desfun sign (x) (if (>= x 0) '+ '-))

;; ===========================================================================
;; Strings
;; ===========================================================================

(desfun string-empty? (s) (eq? 0 (string-length s)))
(desfun intern (str) (string->symbol str))
(desfun concat (&rest args) (apply 'string-append args))

(desfun string->bytes (s)
  (let ((res '()))
    (string-for-each s (lambda (char)
                       (set! res (cons char res))))
    (reverse res)))

(desfun string-upcase (s) (string-to-upper s))
(desfun string-downcase (s) (string-to-lower s))
(desfun string-capitalize (s) (string-titelize s))

;; ===========================================================================
;; Logging
;; ===========================================================================

(desfun log-info (fmt-str &rest args)
  (apply 'log (cons 'info (cons fmt-str null))))

(desfun log-warn (fmt-str &rest args)
  (apply 'log (cons 'warn (cons fmt-str null))))

(desfun log-error (fmt-str &rest args)
  (apply 'log (cons 'error (cons fmt-str null))))

;; ===========================================================================
;; Getters & Setters
;; ===========================================================================

(defsmacro setf (place value)
  (cond
    ((symbol? place) `(set! ,place ,value))
    ((eq? (car place) '->) `(mem-set! ,place ,value))
    (#t (let ((setter (get-setter (car place))))
         (if setter
             `(,setter ,@(cdr place) ,value)
             (error (fmt #f "Unknown place for setf: {}" (car place)) ()))))))

(defsmacro incf (place &rest args)
  (let ((delta (if (null? args) 1 (car args))))
    `(setf ,place (+ ,place ,delta))))

(defsmacro decf (place &rest args)
  (let ((delta (if (null? args) 1 (car args))))
    `(setf ,place (- ,place ,delta))))

(defsetf 'car 'set-car!)
(defsetf 'cdr 'set-cr!)
(defsetf 'get 'hash-table-set!)
(defsetf 'vector-ref 'vector-set!)
(defsetf 'vector-get 'vector-set!)

;; ===========================================================================
;; Asserts & Errors
;; ===========================================================================

(defsmacro assert (condition &optional message)
  `(if (not ,condition)
       (error (if ,message ,message "Assertion failed!"))
       #t))

(defsmacro check-type (var predicate)
  `(assert (,predicate ,var)
           (fmt #f "Variable `{}` failed type check `{}` because value: `{}` of type {}"
                ',var ',predicate ,var (type-of ,var))))

;; ===========================================================================
;; Object navigation
;; ===========================================================================

(desfun identity (item) item)

;; ===========================================================================
;; Structures (SOOT level: defstruct)
;; ===========================================================================

(defsmacro defstruct (name &rest fields)
  (when (not (list? fields)) (error "expected list with arguments"))
  (let* ((struct-name-sym name)
          (doc? (string? (car fields)))
          (fields (if doc? (cdr fields) fields))
          (struct-name-str (symbol->string name))
          (constructor (intern (concat "make-" struct-name-str)))
          (normalized (map (lambda (f) (if (pair? f) f (list f null))) fields)))
    `(begin
       (desfun ,constructor (&key ,@normalized)
          (let ((h (make-hash-table :type ',struct-name-sym)))
            ,@(map (lambda (f-spec)
                    (let* ((f-name (car f-spec))
                            (key (intern (concat ":" (symbol->string f-name)))))
                      `(hash-table-set! h ,key ,f-name)))
                  normalized)
            h))

       (desfun ,(intern (concat struct-name-str "?")) (obj)
         (eq? (type-of obj) ',struct-name-sym))

       ,@(flat-map (lambda (f-spec)
                  (let* ((f-name (car f-spec))
                         (f-str (symbol->string f-name))
                         (getter-name (intern (concat struct-name-str "-" f-str)))
                         (setter-name (intern (concat struct-name-str "-" f-str "!")))
                         (key (intern (concat ":" f-str))))
                    (list
                      `(desfun ,getter-name (instance)
                         (hash-table-ref instance ,key))
                      `(desfun ,setter-name (instance value)
                         (hash-table-set! instance ,key value))
                      `(defsetf ',getter-name ',setter-name))))
                normalized)
       ',struct-name-sym)))

(desfun build-struct-from-plist (args)
  "Turn (:type 'imm :value 10) into ((:type . imm) (:value . 10))"
  (if (null? args)
      '()
      (cons (cons (car args) (cadr args))
            (build-struct-from-plist (cddr args)))))

;; ===========================================================================
;; Serialization
;; ===========================================================================

(defsmacro with-archive (ar buffer direction &rest body)
  `(let ((,ar (make-memory-archive ,buffer :write ,(eq direction :write))))
     (unwind-protect
         (progn ,@body)
       (archive-close! ,ar))))

;; ===========================================================================
;; Bootstrap SOOT Compiler macro system
;; ===========================================================================

;; goal macro to define a goal macro
(defgmacro defmacro (name args &rest body)
  `(begin
     ,(if (and (> (length body) 1) (string? (first body)))
          ;; then it's a docstring and we ignore it.
          `(begin
            (update-macro-metadata ,name ,(first body) ,args)
            (seval (defgmacro ,name ,args ,@(cdr body))))
          ;; otherwise don't ignore it.
          `(begin
            (update-macro-metadata ,name "" ,args)
            (seval (defgmacro ,name ,args ,@body)))
          )
     )
  )

;; ===========================================================================
;; Load marker
;; ===========================================================================
(define *soot-lib-loaded* #t)