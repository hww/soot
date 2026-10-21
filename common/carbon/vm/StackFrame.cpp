#include "carbon/vm/StackFrame.hpp"
#include "carbon/file/Export.hpp"

#include <cstring>

namespace carbon {

    StackFrame::StackFrame() { initialize_registers(); }

    StackFrame::StackFrame(ScriptLambda *functionDesc, std::shared_ptr<StackFrame> parent,
                           FrameType frame_type, StringId name)
        : name(name), frame_type(frame_type), byte_code(functionDesc),
          code_ptr(functionDesc ? reinterpret_cast<Instruction *>(functionDesc->m_pInstruction)
                                : nullptr),
          data_ptr(functionDesc ? functionDesc->m_pSymbols : nullptr), parent(parent), pc(0),
          argc(0), ret_num(0) {
        initialize_registers();
    }

    // ------------------------------------------------------------------------
    // Child management
    // ------------------------------------------------------------------------

    void StackFrame::attach_child(std::shared_ptr<StackFrame> child) {
        if (!child) return;

        child->parent = shared_from_this();
        child->m_nextSibling.reset();

        if (!m_firstChild) {
            m_firstChild = std::move(child);
            return;
        }

        StackFrame *cursor = m_firstChild.get();
        while (cursor->m_nextSibling) { cursor = cursor->m_nextSibling.get(); }
        cursor->m_nextSibling = std::move(child);
    }

    void StackFrame::detach_child(std::shared_ptr<StackFrame> child) {
        if (!child) return;

        if (m_firstChild == child) {
            m_firstChild = child->m_nextSibling;
            child->m_nextSibling.reset();
            child->parent.reset();
            return;
        }

        StackFrame *cursor = m_firstChild.get();
        while (cursor && cursor->m_nextSibling != child) { cursor = cursor->m_nextSibling.get(); }
        if (cursor && cursor->m_nextSibling) {
            cursor->m_nextSibling = child->m_nextSibling;
            child->m_nextSibling.reset();
            child->parent.reset();
        }
    }

    void StackFrame::free_children() {
        // Detach the whole chain first, so that exit() cannot see a
        // half-broken list.
        std::shared_ptr<StackFrame> child = std::move(m_firstChild);
        m_firstChild.reset();

        while (child) {
            std::shared_ptr<StackFrame> next = std::move(child->m_nextSibling);
            child->m_nextSibling.reset();
            child->parent.reset();

            child->free_children(); // depth-first
            child->exit();
            child.reset();

            child = std::move(next);
        }
    }

    // ------------------------------------------------------------------------
    // Registers
    // ------------------------------------------------------------------------

    void StackFrame::initialize_registers() {
        for (u32 i = 0; i < MAX_REGISTERS; ++i) registers[i] = Variant();
    }

    Variant &StackFrame::get_register(u32 i) {
        ASSERT_FORMAT(i < MAX_REGISTERS, "reg {}", i);
        return registers[i];
    }
    const Variant &StackFrame::get_register(u32 i) const {
        ASSERT_FORMAT(i < MAX_REGISTERS, "reg {}", i);
        return registers[i];
    }
    void StackFrame::set_register(u32 i, const Variant &v) { get_register(i) = v; }
    void StackFrame::set_register(u32 i, Variant &&v) { get_register(i) = std::move(v); }

    Variant &StackFrame::get_argument(u32 i) {
        ASSERT_FORMAT(i < MAX_ARGS, "arg {}", i);
        return registers[ARG_REGISTERS_OFFSET + i];
    }
    const Variant &StackFrame::get_argument(u32 i) const {
        ASSERT_FORMAT(i < MAX_ARGS, "arg {}", i);
        return registers[ARG_REGISTERS_OFFSET + i];
    }
    void StackFrame::set_argument(u32 i, const Variant &v) { get_argument(i) = v; }
    void StackFrame::set_argument(u32 i, Variant &&v) { get_argument(i) = std::move(v); }

    Variant &StackFrame::get_local(u32 i) {
        ASSERT_FORMAT(i < MAX_LOCALS, "loc {}", i);
        return registers[LOCAL_REGISTERS_OFFSET + i];
    }
    const Variant &StackFrame::get_local(u32 i) const {
        ASSERT_FORMAT(i < MAX_LOCALS, "loc {}", i);
        return registers[LOCAL_REGISTERS_OFFSET + i];
    }
    void StackFrame::set_local(u32 i, const Variant &v) { get_local(i) = v; }
    void StackFrame::set_local(u32 i, Variant &&v) { get_local(i) = std::move(v); }

    // ------------------------------------------------------------------------
    // Instruction fetch
    // ------------------------------------------------------------------------

    Instruction StackFrame::get_this_instruction() {
        ASSERT_MSG(code_ptr != nullptr, "No code pointer set");
        ASSERT_FORMAT(pc < get_code_size(), "pc {}", pc);
        return code_ptr[pc];
    }

    Instruction StackFrame::get_next_instruction() {
        ASSERT_MSG(code_ptr != nullptr, "No code pointer set");
        ASSERT_FORMAT(pc < get_code_size(), "pc {}", pc);
        return code_ptr[pc++];
    }

    void StackFrame::jump_to(u32 new_pc) {
        ASSERT_FORMAT(new_pc < get_code_size(), "jump {}", new_pc);
        pc = new_pc;
    }

    u32 StackFrame::get_code_size() const {
        if (!code_ptr) return 0;
        return 1024; // temporary, see original comment
    }

    // ------------------------------------------------------------------------
    // Data access
    // ------------------------------------------------------------------------

    i64 StackFrame::get_static_int(u32 offset) const {
        ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
        return static_cast<i64>(data_ptr[offset]);
    }

    f64 StackFrame::get_static_float(u32 offset) const {
        ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
        u64 raw = data_ptr[offset];
        f64 f;
        std::memcpy(&f, &raw, sizeof(f64));
        return f;
    }

    void *StackFrame::get_static_pointer(u32 offset) const {
        ASSERT_MSG(data_ptr != nullptr, "No data pointer set");
        return reinterpret_cast<void *>(data_ptr[offset]);
    }

    // ------------------------------------------------------------------------
    // Debug
    // ------------------------------------------------------------------------

    std::string StackFrame::to_string() const {
        return fmt::format("StackFrame(name='{}', pc={}, argc={}, ret={})", name.to_string(), pc,
                           argc, ret_num);
    }

    void StackFrame::dump_registers() const {
        for (u32 i = 0; i < MAX_REGISTERS; ++i) {
            if (!registers[i].is_null()) { lg::debug("  r{:2d}: {}", i, registers[i].to_string()); }
        }
    }

} // namespace carbon