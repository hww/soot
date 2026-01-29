#include "Aliasable.hpp"

// ============================================================================
// Aliasable
// ============================================================================

Object Aliasable::make_step_alias(const Object& key) {

    if (!key.is_symbol()) return Object::make_empty_list();
    
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

// ============================================================================
// StaticBuffer
// ============================================================================

// --- Реализация Aliasable для Лиспа ---
void StaticBuffer::define_all_aliases() {
    // Свойства самого буфера
    define_alias("origin", [](Aliasable* s) {
        return Object::make_integer(static_cast<StaticBuffer*>(s)->m_origin);
    });
    define_alias("size", [](Aliasable* s) {
        return Object::make_integer(static_cast<StaticBuffer*>(s)->m_data.size());
    });
    define_alias("type", [](Aliasable* s) {
        return EnvContext::make_symbol(static_cast<StaticBuffer*>(s)->m_type_name);
    });
    // Можно добавить "data", который вернет массив Лиспа, если нужно
}
