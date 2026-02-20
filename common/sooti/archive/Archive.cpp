#include "Archive.hpp"
#include "type_system/Type.hpp"

namespace script {

// ============================================================
// Archive
// ============================================================

Archive::~Archive() {}
void Archive::serialize(void *v, int length) {
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
    return INDEX_NONE;
}

int Archive::total_size() {
    return INDEX_NONE;
}

bool Archive::at_end() {
    int pos = tell();
    return pos != INDEX_NONE && pos >= total_size();
}

void Archive::seek(int in_pos) {
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

// ============================================================
//	Crc32Value.
// ============================================================

Archive &operator<<(Archive &ar, Crc32Value &c) {
    if (ar.is_loading()) {
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
    if (ar.is_loading()) {
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
    if (Ar.is_loading()) {
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