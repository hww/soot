#include "Archive.hpp"
#include "type_system/Config.hpp"
#include "common/CommonTypes.hpp"
#include <stdexcept>

namespace script {

// ============================================================
// Archive
// ============================================================

Archive::~Archive() {}
void Archive::serialize_obj(void *v, int length) {
    (void)v;
    (void)length;
}
// Archive &Archive::operator<<(FName &Name) {
//     return *this;
// }
// Archive &Archive::operator<<(SFieldObject *&Object) {
//     return *this;
// }
int Archive::tell() {
    return INVALID_INDEX;
}

int Archive::total_size() {
    return INVALID_INDEX;
}

bool Archive::at_end() {
    int pos = tell();
    return pos != INVALID_INDEX && pos >= total_size();
}

void Archive::seek(size_t in_pos) {
    (void)in_pos;
}

void Archive::precache(int hint_count) {
    (void)hint_count;
}

void Archive::flush() {}

bool Archive::close() {
    return !m_is_error;
}

bool Archive::get_error() {
    return m_is_error;
}

std::string Archive::print() const {
    return m_is_reading ? "#<archive reading>"
                        : (m_is_writing ? "#<archive writing>" : "#<archive>");
}

Object Archive::inspect() const {
    return pretty_print::build_list(
        pretty_print::build_list(Object::make_symbol(":type"), Object::make_string(class_name())),
        pretty_print::build_list(Object::make_symbol(":version"), Object::make_integer(m_version)),
        pretty_print::build_list(Object::make_symbol(":is-reading"),
                                 Object::make_boolean(m_is_reading)),
        pretty_print::build_list(Object::make_symbol(":is-writing"),
                                 Object::make_boolean(m_is_writing)),
        pretty_print::build_list(Object::make_symbol(":is-error"),
                                 Object::make_boolean(m_is_error)),
        pretty_print::build_list(Object::make_symbol(":is-big-endian"),
                                 Object::make_boolean(is_big_endian)));
}

// ============================================================
//	Crc32Value.
// ============================================================

Archive &operator<<(Archive &ar, CompactCrc32 &c) {
    if (ar.is_reading()) {
        switch (TypeConfig::crc_value_size) {
        case 1: {
            uint8_t v;
            ar << v;
            c.value = v;
            break;
        }
        case 2: {
            uint16_t v;
            ar << v;
            c.value = v;
            break;
        }
        case 4: {
            uint32_t v;
            ar << v;
            c.value = v;
            break;
        }
        default:
            throw std::runtime_error("Unimplemented CRC size");
        }
    } else {
        switch (TypeConfig::crc_value_size) {
        case 1: {
            uint8_t v = c.value;
            ar << v;
            break;
        }
        case 2: {
            uint16_t v = c.value;
            ar << v;
            break;
        }
        case 4: {
            uint32_t v = c.value;
            ar << v;
            break;
        }
        default:
            throw std::runtime_error("Unimplemented CRC size");
        }
    }
    return ar;
}

// ============================================================
//	Crc32Value.
// ============================================================

Archive &operator<<(Archive &ar, CompactPointer &c) {
    if (ar.is_reading()) {
        switch (TypeConfig::pointer_size) {
        case 2: {
            uint16_t v;
            ar << v;
            c.value = v;
            break;
        }
        case 4: {
            uint32_t v;
            ar << v;
            c.value = v;
            break;
        }
        default:
            throw std::runtime_error("Unimplemented CRC size");
        }
    } else {
        switch (TypeConfig::crc_value_size) {
        case 2: {
            uint16_t v = c.value;
            ar << v;
            break;
        }
        case 4: {
            uint32_t v = c.value;
            ar << v;
            break;
        }
        case 8: {
            uint64_t v = c.value;
            ar << v;
            break;
        }
        default:
            throw std::runtime_error("Unimplemented CRC size");
        }
    }
    return ar;
}

// ============================================================
//	CompactIndex.
// ============================================================

Archive &operator<<(Archive &Ar, CompactIndex &I) {
    if (Ar.is_reading()) {
        int           val;
        unsigned char b;
        bool          neg = false;

        //	1-st byte.
        Ar << b;
        if (b & 0x40)
            neg = true;
        val = b & 0x3f;

        if (b & 0x80) {
            //	2-nd byte.
            Ar << b;
            val |= (b & 0x7f) << 6;

            if (b & 0x80) {
                //	3-rd byte.
                Ar << b;
                val |= (b & 0x7f) << 13;

                if (b & 0x80) {
                    //	4-th byte.
                    Ar << b;
                    val |= (b & 0x7f) << 20;

                    if (b & 0x80) {
                        Ar << b;
                        val |= b << 27;
                    }
                }
            }
        }

        //	Set negative.
        if (neg)
            val = -val;

        I.value = val;
    } else {
        unsigned char b;
        int           idx = I.value;

        //	Check for negateve numbers.
        bool neg = false;
        if (idx < 0) {
            neg = true;
            idx = -idx;
        }

        //	1-st byte.
        b = idx & 0x3f;
        if (neg)
            b |= 0x40;
        if (idx > 0x3f)
            b |= 0x80;
        Ar << b;

        if (idx > 0x3f) {
            //	2-nd byte.
            b = (idx >> 6) & 0x7f;
            if (idx > 0x1fff)
                b |= 0x80;
            Ar << b;

            if (idx > 0x1fff) {
                //	3-rd byte.
                b = (idx >> 13) & 0x7f;
                if (idx > 0xfffff)
                    b |= 0x80;
                Ar << b;

                if (idx > 0xfffff) {
                    //	4-th byte.
                    b = (idx >> 20) & 0x7f;
                    if (idx > 0x7ffffff)
                        b |= 0x80;
                    Ar << b;

                    if (idx > 0x7ffffff) {
                        //	5-th byte.
                        b = idx >> 27;
                        Ar << b;
                    }
                }
            }
        }
    }
    return Ar;
}

} // namespace script