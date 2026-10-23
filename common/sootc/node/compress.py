import os
import re

# Имя итогового файла
OUTPUT_FILE = "combined_code.txt"

def clean_csharp_code(content):
    """
    Удаляет из C# кода всё лишнее для ИИ: комментарии, 
    директивы using и лишние пустые строки.
    """
    # 1. Удаляем многострочные комментарии /* ... */
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    
    # 2. Удаляем однострочные комментарии // ...
    # Используем lookbehind, чтобы случайно не удалить // внутри URL-строк
    content = re.sub(r'(?<!:)//.*', '', content)
    
    lines = content.splitlines()
    cleaned_lines = []
    
    for line in lines:
        stripped = line.strip()
        
        # 3. Удаляем директивы using (ИИ обычно не важны импорты)
        if stripped.startswith("using ") and stripped.endswith(";"):
            continue
            
        # 4. Пропускаем абсолютно пустые строки
        if not stripped:
            continue
            
        cleaned_lines.append(line)
        
    return "\n".join(cleaned_lines)

def main():
    # Текущая рабочая директория (PWD)
    pwd = os.getcwd()
    
    with open(OUTPUT_FILE, "w", encoding="utf-8") as outfile:
        # Рекурсивный обход от PWD
        for root, _, files in os.walk(pwd):
            for file in files:
                if file.endswith(".cpp") or file.endswith(".hpp") or file.endswith(".sot"):
                    full_path = os.path.join(root, file)
                    
                    # Получаем относительный путь от PWD
                    rel_path = os.path.relpath(full_path, pwd)
                    
                    try:
                        with open(full_path, "r", encoding="utf-8") as infile:
                            content = infile.read()
                        
                        # Сжимаем код
                        #compressed_content = clean_csharp_code(content)
                        compressed_content = content
                        
                        # Записываем заголовок с путем и очищенный код
                        outfile.write(f"\n// === FILE: {rel_path} ===\n")
                        outfile.write(compressed_content)
                        outfile.write("\n")
                        
                        print(f"Обработан: {rel_path}")
                        
                    except Exception as e:
                        print(f"Ошибка при чтении {rel_path}: {e}")

    print(f"\nГотово! Все файлы собраны и сжаты в: {OUTPUT_FILE}")

if __name__ == "__main__":
    main()

