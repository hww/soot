
#pragma once

#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include "common/CommonTypes.hpp"

namespace script {

/*!
 * Archive class. Used for loading, saving, and garbage collecting
 * in a byte order neutral way.
 */
class Archive : public NativeObject {
  public:
    // Archive interface.
    Archive()
        : m_version(0), m_is_reading(false), m_is_writing(false), m_is_persistent(false),
          m_is_error(false) {}
    Archive(bool is_loading, bool is_saving, bool presistant)
        : m_version(0), m_is_reading(is_loading), m_is_writing(is_saving),
          m_is_persistent(presistant), m_is_error(false) {}
    virtual ~Archive();

    virtual void serialize_obj(void *v, int length);
    // virtual Archive &operator<<(FName &Name);
    // virtual Archive &operator<<(SFieldObject *&Object);
    virtual int  tell();
    virtual int  total_size();
    virtual bool at_end();
    virtual void seek(size_t inPos);
    virtual void precache(int hintCount);
    virtual void flush();
    virtual bool close();
    virtual bool get_error();

    //	Hardcoded datatype routines that may not be overridden.
    Archive &byte_order_serialize(void *v, int length) {
#if defined __i386__ || defined _M_IX86
        serialize(v, length);
#else
        if (is_big_endian && m_is_persistent) {
            //	Transferring between memory and file, so flip the byte order.
            for (int i = length - 1; i >= 0; i--)
                serialize_obj((uint8_t *)v + i, 1);
        } else {
            //	Transferring around within memory, so keep the byte order.
            serialize_obj(v, length);
        }
#endif
        return *this;
    }

    // Status accessors.
    int version() {
        return m_version;
    }
    bool is_reading() {
        return m_is_reading;
    }
    bool is_writing() {
        return m_is_writing;
    }
    bool is_persistent() {
        return m_is_persistent;
    }
    bool is_error() {
        return m_is_error;
    }

    std::string print() const override;
    Object      inspect() const override;

    std::string full_class_name() const override {
        return "Archive";
    }

    std::string class_name() const override {
        return "archive";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == Archive::type_name_obj() || NativeObject::is_class_name(name);
    }

    // Friend archivers.
    friend Archive &operator<<(Archive &ar, bool &b) {
        ar.serialize_obj(&b, 1);
        return ar;
    }
    friend Archive &operator<<(Archive &ar, char &c) {
        ar.serialize_obj(&c, 1);
        return ar;
    }
    friend Archive &operator<<(Archive &ar, uint8_t &b) {
        ar.serialize_obj(&b, 1);
        return ar;
    }
    friend Archive &operator<<(Archive &ar, signed char &b) {
        ar.serialize_obj(&b, 1);
        return ar;
    }
    friend Archive &operator<<(Archive &ar, uint16_t &w) {
        ar.byte_order_serialize(&w, sizeof(w));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, int16_t &s) {
        ar.byte_order_serialize(&s, sizeof(s));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, uint32_t &d) {
        ar.byte_order_serialize(&d, sizeof(d));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, int32_t &i) {
        ar.byte_order_serialize(&i, sizeof(i));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, uint64_t &d) {
        ar.byte_order_serialize(&d, sizeof(d));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, int64_t &i) {
        ar.byte_order_serialize(&i, sizeof(i));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, float &f) {
        ar.byte_order_serialize(&f, sizeof(f));
        return ar;
    }
    friend Archive &operator<<(Archive &ar, double &f) {
        ar.byte_order_serialize(&f, sizeof(f));
        return ar;
    }

  protected:
    // Status variables.
    int  m_version;
    bool m_is_reading;
    bool m_is_writing;
    bool m_is_persistent;
    bool m_is_error;
    bool is_big_endian;
};
/*!
 * Class for serializing objects in a compactly, mapping small values
 * to fewer bytes.
 */
class CompactIndex {
  public:
    int             value;
    friend Archive &operator<<(Archive &ar, CompactIndex &i);

    operator uint32_t() const {
        return static_cast<uint32_t>(value);
    }
    operator int() const {
        return static_cast<int>(value);
    }
};

class CompactPointer {
  public:
    uint64_t        value;
    friend Archive &operator<<(Archive &ar, CompactPointer &i);
};

class CompactCrc32 {
  public:
    uint32_t        value;
    friend Archive &operator<<(Archive &ar, CompactCrc32 &i);

    operator uint32_t() const {
        return value;
    }
    operator int() const {
        return static_cast<int>(value);
    }
};

/*!
 *	Archive constructor.
 */
template <class T> T Arctor(Archive &ar) {
    T tmp;
    ar << tmp;
    return tmp;
}
} // namespace script