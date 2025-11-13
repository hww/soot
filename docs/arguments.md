Usage: scheme [options] [filename]
       scheme [options] -e "expression"
       scheme [options] -r "expression"

Options:
  --help, -h              Show this help message
  
Input modes (mutually exclusive):
  -e, --eval EXPR         Evaluate expression in interpreter
  -r, --run EXPR          Compile and run expression
  filename                Read and execute file (default mode)

Output control:
  --tokens [filename]     Print tokens to screen or file
  --ast [filename]        Print AST to screen or file  
  --bytecode [filename]   Print bytecode to screen or file
  --string-info           Print string database info

Execution mode:
  --compile              Compile only (default for files)
  --run                  Compile and execute (default for -r)
  --eval                 Interpret only (default for -e)



# Помощь
./scheme --help

# Разные режимы выполнения выражений
./scheme -e "(+ 1 2)"                    # Интерпретация
./scheme -r "(* 3 4)"                    # Компиляция + выполнение
./scheme --eval -e "(print \"hello\")"   # Явная интерпретация

# Работа с файлами
./scheme program.scm                      # Выполнить файл
./scheme --compile program.scm           # Только компиляция
./scheme --eval program.scm              # Интерпретация файла

# Отладочная информация
./scheme --tokens program.scm            # Токены на экран
./scheme --ast program.scm               # AST на экран  
./scheme --bytecode program.scm          # Байткод на экран
./scheme --ast ast.txt --bytecode bc.txt # В файлы

# Комбинации
./scheme --tokens --ast -e "(define x 42)" # Токены + AST выражения
./scheme --compile --bytecode file.scm    # Компиляция + показ байткода
