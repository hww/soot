#pragma once

#include "Object.hpp"
#include "StaticBuffer.hpp"

#include <fmt/format.h>

namespace script {

class StaticWriter : public HeapObject {
  private:
    size_t                        m_position;
    std::shared_ptr<StaticBuffer> m_buffer;

  public:
    StaticWriter(const std::shared_ptr<StaticBuffer> &buffer);

    /**
     * Основной метод: "Зарезервировать" место под тип и вернуть ячейку для записи.
     */
    Object allocate(Type *type);

    // Методы управления
    void seek(size_t pos) {
        m_position = std::min(pos, m_buffer->size());
    }
    size_t tell() const {
        return m_position;
    }
    void align(size_t a) {
        m_position = (m_position + a - 1) & ~(a - 1);
    }
    size_t remaining() const {
        return m_buffer->size() - m_position;
    }
    std::shared_ptr<script::StaticBuffer> get_buffer() {
        return m_buffer;
    }

    // Инспекция (упрощенная)
    std::string print() const override {
        return fmt::format("#<static-writer :address {}/{} :used {:.1f}%>", m_position,
                           m_buffer->size(), (float)m_position / m_buffer->size() * 100.f);
    }

    Object make_step_accessor(const Object &key) override {
        std::string name = key.to_std_string();
        if (name == ".size")
            return Object::make_integer(m_buffer->size());
        if (name == ".origin")
            return Object::make_integer(m_buffer->origin());
        if (name == ".type")
            return Object::make_string(m_buffer->type_name());
        if (name == ".position")
            return Object::make_integer(m_position);
        if (name == ".remaining")
            return Object::make_integer(remaining());

        return Object::make_none();
    }

    Object inspect() const override {
        ListBuilder lb{};
        lb.add_symbol("static-writer");
        lb.add_keyword(":cursor");
        lb.add_integer(m_position);

        return lb.build();
    }

    std::string type_name() const override {
        return object_type_to_string(ObjectType::STATIC_WRITER);
    }
    std::string class_name() const override {
        return "StaticWriter";
    }
};

} // namespace script