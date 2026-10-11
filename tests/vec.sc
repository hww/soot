(deftype cvector2 (structure)
  ((x float :offset 0)
   (y float :offset 4)
   (z float :offset 8)
   (w float :offset 12)))

(defun make-vec ()
  (new 'global 'cvector2 1.0 2.0 3.0 4.0))
