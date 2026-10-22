#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/Instructions.hpp"
#include "file/DCScript.hpp"
#include "lib/StringId.hpp"
#include "util/Assert.hpp"
#include "util/Log.hpp"

#include <functional>
#include <memory>
#include <ostream>
#include <string>

namespace carbon {

    class VirtualMachine;

    // ============================================================================
    // StackFrame — base for every execution frame
    // ============================================================================
    //
    // Ownership model:
    //
    //   parent       : weak_ptr  — the frame does NOT own its parent
    //   m_firstChild : shared_ptr — the frame OWNS its children
    //   m_nextSibling: shared_ptr — the frame OWNS its next sibling in the
    //                              parent's child list
    //
    // A frame's child list is a singly-linked list threaded through
    // m_nextSibling. The parent owns the first child (m_firstChild);
    // each child owns the next. free_children() walks the list and
    // resets every link, recursively. No cycles: parent is weak.
    //
    // Two kinds of children:
    //
    //   * Temporary (push/pop): a synchronous call frame. It lives in
    //     stack_frame_top, not in m_firstChild. It is destroyed when
    //     the call returns (Return).
    //
    //   * Long-lived (attach_child): a track, a coroutine, a state.
    //     It lives in m_firstChild of its owner. It is destroyed when
    //     the owner is destroyed or free_children() is called.
    //
    struct StackFrame : public std::enable_shared_from_this<StackFrame> {
        enum class FrameType { CATCH, PROTECT, STATE, TRACK, GENERIC };

        // GC Debugging
        u64 mark_id = 0;

        // --------------------------------------------------------------------
        // Identity
        // --------------------------------------------------------------------
        StringId  name = StringIds::none;
        FrameType frame_type = FrameType::GENERIC;

        // --------------------------------------------------------------------
        // Execution state
        // --------------------------------------------------------------------
        ScriptLambda *byte_code = nullptr;
        Instruction  *code_ptr = nullptr;
        u64          *data_ptr = nullptr;

        u32 pc = 0;
        u32 argc = 0;
        u32 ret_num = 0;

        // --------------------------------------------------------------------
        // Tree links
        // --------------------------------------------------------------------
        std::shared_ptr<StackFrame> parent;
        std::shared_ptr<StackFrame> m_firstChild;
        std::shared_ptr<StackFrame> m_nextSibling;

        // --------------------------------------------------------------------
        // Registers
        // --------------------------------------------------------------------
        Variant registers[MAX_REGISTERS];

        StackFrame();
        StackFrame(ScriptLambda *functionDesc, std::shared_ptr<StackFrame> parent,
                   FrameType frame_type = FrameType::GENERIC, StringId name = StringIds::none);
        virtual ~StackFrame() = default;

        // --------------------------------------------------------------------
        // Lifecycle hooks
        // --------------------------------------------------------------------
        virtual void exit() {}
        virtual void on_throw() {}

        // --------------------------------------------------------------------
        // Parent access (weak_ptr -> shared_ptr via lock)
        // --------------------------------------------------------------------
        std::shared_ptr<StackFrame> get_parent() const { return parent; }

        // --------------------------------------------------------------------
        // Child management
        // --------------------------------------------------------------------
        /// Attach `child` as the last child of this frame.
        /// The child's parent is set to this frame. The child's
        /// m_nextSibling is set to nullptr.
        void attach_child(std::shared_ptr<StackFrame> child);

        /// Remove `child` from this frame's child list.
        /// The child's parent and m_nextSibling are cleared.
        void detach_child(std::shared_ptr<StackFrame> child);

        /// Recursively destroy every child (and their subtrees).
        /// Calls exit() on each child before releasing it.
        void free_children();

        // --------------------------------------------------------------------
        // Typed search
        // --------------------------------------------------------------------
        /// Find the nearest frame of type T in the parent chain (this included).
        template <typename T> T *find_frame_of_type() {
            StackFrame *cursor = this;
            while (cursor) {
                if (auto *typed = dynamic_cast<T *>(cursor)) return typed;
                cursor = cursor->get_parent().get();
            }
            return nullptr;
        }

        template <typename T> const T *find_frame_of_type() const {
            return const_cast<StackFrame *>(this)->find_frame_of_type<T>();
        }

        /// Find a child of type T in this frame's direct child list.
        template <typename T> T *find_child_of_type() {
            StackFrame *cursor = m_firstChild.get();
            while (cursor) {
                if (auto *typed = dynamic_cast<T *>(cursor)) return typed;
                cursor = cursor->m_nextSibling.get();
            }
            return nullptr;
        }

        // --------------------------------------------------------------------
        // Register access (unchanged, shortened here)
        // --------------------------------------------------------------------
        Variant       &get_register(u32 index);
        const Variant &get_register(u32 index) const;
        void           set_register(u32 index, const Variant &value);
        void           set_register(u32 index, Variant &&value);

        Variant       &get_argument(u32 index);
        const Variant &get_argument(u32 index) const;
        void           set_argument(u32 index, const Variant &value);
        void           set_argument(u32 index, Variant &&value);

        Variant       &get_local(u32 index);
        const Variant &get_local(u32 index) const;
        void           set_local(u32 index, const Variant &value);
        void           set_local(u32 index, Variant &&value);

        u32 get_registers_count() const { return MAX_REGISTERS; }

        // --------------------------------------------------------------------
        // Instruction fetch
        // --------------------------------------------------------------------
        Instruction get_this_instruction();
        Instruction get_next_instruction();
        void        jump_to(u32 new_pc);
        u32         get_code_size() const;

        // --------------------------------------------------------------------
        // Data access
        // --------------------------------------------------------------------
        i64   get_static_int(u32 offset) const;
        f64   get_static_float(u32 offset) const;
        void *get_static_pointer(u32 offset) const;

        // --------------------------------------------------------------------
        // Debug
        // --------------------------------------------------------------------
        std::string to_string() const;
        void        dump_registers() const;

    private:
        void initialize_registers();
    };

    inline std::ostream &operator<<(std::ostream &os, const StackFrame &frame) {
        return os << frame.to_string();
    }

} // namespace carbon