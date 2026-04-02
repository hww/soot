#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/files/FunctionDesc.hpp"

namespace carbon::files {

void RelocatableBuffer::add_function(
    const std::string& name,
    const std::vector<vm::Instruction>& code,
    const std::vector<u8>& data,
    const std::vector<SourceLocation>& debug_info,
    SymbolFlags flags) {
    
    (void)flags; 

    // Заполняем FunctionDesc
    FunctionDesc desc;
    desc.code_count = static_cast<u32>(code.size());
    desc.data_size = static_cast<u32>(data.size());
    desc.debug_count = static_cast<u32>(debug_info.size());
    desc.reserved = 0;
    desc.owner_module = nullptr;
    desc.code_ptr = nullptr;
    desc.data_ptr = nullptr;
    desc.debug_ptr = nullptr;
    
    // Записываем заголовок
    u32 header_start = bytes_.size();
    add_bytes(&desc, sizeof(FunctionDesc));
    
    // Отмечаем relocatable поля
    if (!code.empty()) {
        u32 code_ptr_offset = header_start + offsetof(FunctionDesc, code_ptr);
        add_relocatable( code_ptr_offset, Relocation::Type::FILE_RELATIVE, name + "#code");
    }
    
    if (!data.empty()) {
        u32 data_ptr_offset = header_start + offsetof(FunctionDesc, data_ptr);
        add_relocatable(data_ptr_offset, Relocation::Type::FILE_RELATIVE, name + "#data");
    }
    
    if (!debug_info.empty()) {
        u32 debug_ptr_offset = header_start + offsetof(FunctionDesc, debug_ptr);
        add_relocatable(debug_ptr_offset, Relocation::Type::FILE_RELATIVE, name + "#debug");
    }
    
    // Записываем код
    u32 code_start = 0;
    if (!code.empty()) {
        code_start = bytes_.size();
        add_bytes(code.data(), code.size() * sizeof(vm::Instruction));
    }
    
    // Записываем данные
    u32 data_start = 0;
    if (!data.empty()) {
        data_start = bytes_.size();
        add_bytes(data.data(), data.size());
    }
    
    // Записываем debug информацию
    u32 debug_start = 0;
    if (!debug_info.empty()) {
        debug_start = bytes_.size();
        add_bytes(debug_info.data(), debug_info.size() * sizeof(SourceLocation));
    }
    
    // Обновляем указатели в заголовке (пишем напрямую в уже записанные байты)
    if (!code.empty()) {
        Ptr<vm::Instruction>* code_ptr_field = 
            reinterpret_cast<Ptr<vm::Instruction>*>(bytes_.data() + header_start + offsetof(FunctionDesc, code_ptr));
        code_ptr_field->offset = code_start;
    }
    
    if (!data.empty()) {
        Ptr<u8>* data_ptr_field = 
            reinterpret_cast<Ptr<u8>*>(bytes_.data() + header_start + offsetof(FunctionDesc, data_ptr));
        data_ptr_field->offset = data_start;
    }
    
    if (!debug_info.empty()) {
        Ptr<SourceLocation>* debug_ptr_field = 
            reinterpret_cast<Ptr<SourceLocation>*>(bytes_.data() + header_start + offsetof(FunctionDesc, debug_ptr));
        debug_ptr_field->offset = debug_start;
    }
    
    // Сохраняем информацию об определении для последующей добавления в BinaryFileBuilder
    // Это нужно, если мы хотим хранить определения внутри RelocatableBuffer
    // Или можно вернуть структуру DefinitionData наружу
}
}