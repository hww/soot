#include "Accessor.hpp"

namespace script {

// ============================================================================
// Accessor
// ============================================================================

Object Accessor::make_step_alias(const Object& key) {

    if (!key.is_symbol()) return Object::make_null();
    
    std::string sym_name = key.to_std_string(); // Используем метод получения имени
    const auto& props = get_property_map();

    auto it = props.find(sym_name);
    if (it != props.end()) {
        // Мы просто вызываем геттер ПРЯМО СЕЙЧАС.
        // Почему? Потому что это метаданные (размер, оффсет). 
        // Они не меняются динамически в памяти как волатильные данные.
        // Если геттер возвращает MemoryCell - отлично, если Object - тоже.
        return it->second(this);
    }
    
    // Если в мапе свойств не нашли, возвращаем undefined, 
    // чтобы интерпретатор знал, что шага нет.
    return Object::make_undefined();
}

}