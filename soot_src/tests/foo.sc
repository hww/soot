; soot_src/test/foo.sc
(require "bar.sc")
(defun foo-greet () (bar-greet))
(foo-greet)