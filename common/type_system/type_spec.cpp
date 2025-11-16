#include "type_spec.h"
#include <cstdlib>
#include <cstring>

TypeSpec* type_spec_create(const char* base_type) {
    TypeSpec* ts = new TypeSpec();
    ts->base_type = base_type;
    ts->arguments = nullptr;
    ts->arg_count = 0;
    ts->tags = nullptr;
    ts->tag_count = 0;
    return ts;
}

TypeSpec* type_spec_clone(const TypeSpec* src) {
    if (!src) return nullptr;

    TypeSpec* clone = type_spec_create(src->base_type);

    // Клонируем аргументы
    for (size_t i = 0; i < src->arg_count; i++) {
        type_spec_add_arg(clone, type_spec_clone(src->arguments[i]));
    }

    // Клонируем теги
    for (size_t i = 0; i < src->tag_count; i++) {
        type_spec_add_tag(clone, src->tags[i].name, src->tags[i].value);
    }

    return clone;
}

void type_spec_destroy(TypeSpec* ts) {
    if (!ts) return;

    // Освобождаем аргументы
    for (size_t i = 0; i < ts->arg_count; i++) {
        type_spec_destroy(ts->arguments[i]);
    }
    delete[] ts->arguments;

    // Освобождаем теги
    delete[] ts->tags;

    delete ts;
}

void type_spec_add_arg(TypeSpec* ts, TypeSpec* arg) {
    // Простейшая реализация - переаллокация массива
    TypeSpec** new_args = new TypeSpec * [ts->arg_count + 1];

    // Копируем старые аргументы
    for (size_t i = 0; i < ts->arg_count; i++) {
        new_args[i] = ts->arguments[i];
    }

    // Добавляем новый
    new_args[ts->arg_count] = arg;

    // Заменяем массив
    delete[] ts->arguments;
    ts->arguments = new_args;
    ts->arg_count++;
}

void type_spec_add_tag(TypeSpec* ts, const char* name, const char* value) {
    TypeTag* new_tags = new TypeTag[ts->tag_count + 1];

    // Копируем старые теги
    for (size_t i = 0; i < ts->tag_count; i++) {
        new_tags[i] = ts->tags[i];
    }

    // Добавляем новый тег
    new_tags[ts->tag_count].name = name;
    new_tags[ts->tag_count].value = value;

    // Заменяем массив
    delete[] ts->tags;
    ts->tags = new_tags;
    ts->tag_count++;
}

bool type_spec_has_single_arg(const TypeSpec* ts) {
    return ts->arg_count == 1;
}

const TypeSpec* type_spec_get_single_arg(const TypeSpec* ts) {
    if (ts->arg_count != 1) return nullptr;
    return ts->arguments[0];
}

bool type_spec_is_empty(const TypeSpec* ts) {
    return ts->arg_count == 0 && ts->tag_count == 0;
}