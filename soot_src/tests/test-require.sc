;; test-require.sc

;; require external file
(require "test-require-dep.sc")

;; define function with usage from 'dep'
(defun main-fn () (dep-fn))
