#include "carbon/Carbon.hpp"

#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/DCIFile.hpp"
#include "carbon/file/DCScript.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/gc/ArenaManager.hpp"
#include "carbon/gc/TypeRegistry.hpp"
#include "carbon/lib/StringIdManager.hpp"
#include "carbon/vm/VirtualMachine.hpp"

#include "util/Log.hpp"

#include <set>
#include <unordered_map>

namespace carbon {

    namespace {
        thread_local Carbon *t_current = nullptr;
    }

    Carbon *Carbon::current() noexcept { return t_current; }
    bool    Carbon::has_current() noexcept { return t_current != nullptr; }

    Carbon::Scope::Scope(Carbon &c) noexcept : m_previous(t_current) { t_current = &c; }

    Carbon::Scope::~Scope() noexcept { t_current = m_previous; }


    // ------------------------------------------------------------------------
    // ModuleRegistry
    // ------------------------------------------------------------------------
    //
    // Owns every loaded BinaryFile and remembers, for each module, which
    // SIDs it contributed to Globals and which SsTypes it contributed to
    // TypeRegistry. This is what makes unload_module possible.
    class ModuleRegistry {
    public:
        struct Entry {
            std::filesystem::path       path;
            std::string                 logical_name;
            std::unique_ptr<BinaryFile> file;
            std::vector<sid64>          symbol_sids;
            std::vector<sid64>          ss_type_sids;
        };

        bool contains(const std::string &logical_name) const {
            return m_by_name.count(logical_name) != 0;
        }

        bool contains_path(const std::filesystem::path &path) const {
            return m_by_path.count(path) != 0;
        }

        void insert(Entry e) {
            const std::string name = e.logical_name;
            const auto        path = e.path;
            m_by_name[name] = std::move(e);
            m_by_path[path] = name;
        }

        Entry *find(const std::string &logical_name) {
            auto it = m_by_name.find(logical_name);
            return it == m_by_name.end() ? nullptr : &it->second;
        }

        Entry *find_by_path(const std::filesystem::path &path) {
            auto it = m_by_path.find(path);
            if (it == m_by_path.end()) return nullptr;
            return find(it->second);
        }

        void erase(const std::string &logical_name) {
            auto it = m_by_name.find(logical_name);
            if (it == m_by_name.end()) return;
            m_by_path.erase(it->second.path);
            m_by_name.erase(it);
        }

        std::size_t size() const { return m_by_name.size(); }

    private:
        std::unordered_map<std::string, Entry>                 m_by_name;
        std::unordered_map<std::filesystem::path, std::string> m_by_path;
    };

    // ------------------------------------------------------------------------
    // Scheduler (stub for now)
    // ------------------------------------------------------------------------
    //
    // The process model is deliberately not implemented yet. The API
    // exists so that Carbon::spawn_process / tick / kill_process do not
    // have to change when the real scheduler lands.
    class Scheduler {
    public:
        u64 spawn(const std::string &entry, const std::vector<Variant> &args) {
            (void)entry;
            (void)args;
            return 0; // not implemented yet
        }
        void kill(u64) {}
        void tick() {}
        bool empty() const { return true; }
    };

    // ------------------------------------------------------------------------
    // Carbon::Impl
    // ------------------------------------------------------------------------

    struct Carbon::Impl {
        CarbonConfig config;
        bool         initialized = false;
        bool         exit_requested = false;

        std::unique_ptr<gc::ArenaManager> gc_heap;
        std::unique_ptr<VirtualMachine>   vm;
        std::unique_ptr<Globals>          globals;
        std::unique_ptr<ModuleRegistry>   modules;
        std::unique_ptr<Scheduler>        scheduler;
    };

    // ------------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------------

    Carbon::Carbon() : m_impl(std::make_unique<Impl>()) {}
    Carbon::~Carbon() { shutdown(); }

    bool Carbon::initialize(const CarbonConfig &config) {
        if (m_impl->initialized) return true;

        m_impl->config = config;

        // 1. SIDBase. Must come first: everything below registers strings.
        if (!config.sidbase_path.empty()) {
            const bool loaded =
                StringIdManager::instance().load_dconstruct_sidbase(config.sidbase_path);
            if (loaded) {
                lg::info("Carbon: loaded SIDBase from '{}' ({} entries)", config.sidbase_path,
                         StringIdManager::instance().size());
            } else {
                lg::warn("Carbon: failed to load SIDBase '{}'", config.sidbase_path);
            }
        }

        // 2. GC heap. Reserved now, pages committed lazily.
        m_impl->gc_heap =
            std::make_unique<gc::ArenaManager>(config.gc_initial_mb, config.gc_max_mb);

        // 3. TypeRegistry: register the built-in VM types.
        if (config.register_builtin_types) { register_builtin_gc_types(); }

        // 4. VirtualMachine. This also registers the default native
        //    functions (native_print etc.). The language runtime is
        //    free to remove them again in its own setup; Carbon does
        //    not decide what a language looks like.
        m_impl->vm = std::make_unique<VirtualMachine>(this);

        // 5. Globals: THIS context's symbol table. Not a singleton.
        m_impl->globals = std::make_unique<Globals>();

        // 6. ModuleRegistry and Scheduler.
        m_impl->modules = std::make_unique<ModuleRegistry>();
        m_impl->scheduler = std::make_unique<Scheduler>();

        m_impl->initialized = true;
        m_impl->exit_requested = false;

        lg::info("Carbon: initialized (gc {}..{} MB, sidbase='{}')", config.gc_initial_mb,
                 config.gc_max_mb, config.sidbase_path);

                if (t_current != nullptr && t_current != this) {
            lg::error("Carbon::initialize: another Carbon is already active "
                      "in this thread (0x{:X})",
                      reinterpret_cast<uintptr_t>(t_current));
            return false;
        }
        t_current = this;

        return true;
    }

    void Carbon::shutdown() {
        if (!m_impl->initialized) return;

        // Drop processes first, then modules, then symbols, then heap.
        m_impl->scheduler.reset();
        m_impl->modules.reset();
        m_impl->globals.reset();
        m_impl->vm.reset();
        m_impl->gc_heap.reset();

        m_impl->initialized = false;
        
        if (t_current == this) t_current = nullptr;

        lg::info("Carbon: shutdown");
    }

    bool Carbon::is_initialized() const noexcept { return m_impl->initialized; }

    // ------------------------------------------------------------------------
    // Module loading
    // ------------------------------------------------------------------------

    bool Carbon::load_module(const std::filesystem::path &path) {
        if (!m_impl->initialized) {
            lg::error("Carbon::load_module: not initialized");
            return false;
        }

        if (m_impl->modules->contains_path(path)) {
            lg::debug("Carbon::load_module: '{}' already loaded", path.string());
            return true;
        }

        auto result = BinaryFile::from_path(path);
        if (!result) {
            lg::error("Carbon::load_module: cannot read '{}': {}", path.string(), result.error());
            return false;
        }

        auto file = std::make_unique<BinaryFile>(std::move(*result));

        // Build the logical name. For now, stem of the path. When the
        // .dci manifest is available, its logical_path takes priority.
        std::string logical_name = path.stem().string();

        std::vector<sid64> symbol_sids;
        std::vector<sid64> ss_type_sids;

        const DCEntry *table = file->entries();
        const u32      n = file->entry_count();

        for (u32 i = 0; i < n; ++i) {
            const DCEntry &e = table[i];

            // Register SsType entries in the GC TypeRegistry so that
            // gc::alloc knows the layout and mark can find the pointers.
            if (BinaryFile::entry_kind(e) == BinaryFile::EntryKind::SsType) {
                if (const SsType *st = file->entry_as_ss_type(e)) {
                    register_ss_type_in_gc(*st);
                    ss_type_sids.push_back(st->m_name);
                }
            }

            // Register every named entry in THIS context's Globals.
            if (e.m_nameID != 0) {
                m_impl->globals->register_symbol(StringId(e.m_nameID),
                                                 const_cast<void *>(e.m_entryPtr),
                                                 StringId(e.m_typeId), StringId(logical_name));
                symbol_sids.push_back(e.m_nameID);
            }
        }

        ModuleRegistry::Entry entry;
        entry.path = path;
        entry.logical_name = logical_name;
        entry.file = std::move(file);
        entry.symbol_sids = std::move(symbol_sids);
        entry.ss_type_sids = std::move(ss_type_sids);

        m_impl->modules->insert(std::move(entry));

        lg::info("Carbon: loaded '{}' as '{}' ({} symbols, {} ss-types)", path.string(),
                 logical_name, m_impl->modules->find(logical_name)->symbol_sids.size(),
                 m_impl->modules->find(logical_name)->ss_type_sids.size());

        return true;
    }

    bool Carbon::load_module_from_binary(const std::string &logical_name, BinaryFile &&file) {
        if (!m_impl->initialized) {
            lg::error("Carbon::load_module_from_binary: not initialized");
            return false;
        }

        auto owned = std::make_unique<BinaryFile>(std::move(file));

        std::vector<sid64> symbol_sids;
        std::vector<sid64> ss_type_sids;

        const DCEntry *table = owned->entries();
        const u32      n = owned->entry_count();

        for (u32 i = 0; i < n; ++i) {
            const DCEntry &e = table[i];

            if (BinaryFile::entry_kind(e) == BinaryFile::EntryKind::SsType) {
                if (const SsType *st = owned->entry_as_ss_type(e)) {
                    register_ss_type_in_gc(*st);
                    ss_type_sids.push_back(st->m_name);
                }
            }

            if (e.m_nameID != 0) {
                m_impl->globals->register_symbol(StringId(e.m_nameID),
                                                 const_cast<void *>(e.m_entryPtr),
                                                 StringId(e.m_typeId), StringId(logical_name));
                symbol_sids.push_back(e.m_nameID);
            }
        }

        ModuleRegistry::Entry entry;
        entry.path = owned->m_path;
        entry.logical_name = logical_name;
        entry.file = std::move(owned);
        entry.symbol_sids = std::move(symbol_sids);
        entry.ss_type_sids = std::move(ss_type_sids);

        m_impl->modules->insert(std::move(entry));
        return true;
    }

    bool Carbon::load_module_with_imports(const std::filesystem::path              &path,
                                          const std::vector<std::filesystem::path> &search_paths) {
        std::set<std::filesystem::path> already_loaded;
        return load_module_recursive(path, search_paths, already_loaded);
    }

    // Note: load_module_recursive lives as a free function so it can
    // recurse without re-entering the public API. The public API stays
    // a thin wrapper around it.
    bool Carbon::load_module_recursive(const std::filesystem::path              &bin_path,
                                       const std::vector<std::filesystem::path> &search_paths,
                                       std::set<std::filesystem::path>          &already_loaded) {

        namespace fs = std::filesystem;
        std::error_code ec;

        fs::path canonical = fs::weakly_canonical(bin_path, ec);
        if (ec) canonical = bin_path;

        if (already_loaded.count(canonical)) return true;
        already_loaded.insert(canonical);

        if (m_impl->modules->contains_path(canonical)) return true;

        // 1. Read the .dci, if present, and load imports first.
        fs::path dci_path = bin_path;
        dci_path.replace_extension(".dci");

        if (fs::exists(dci_path)) {
            try {
                DCIFile  manifest = DCIFile::parse(dci_path.string());
                fs::path parent_dir = bin_path.parent_path();

                for (const auto &imp : manifest.imports) {
                    const std::string dep_name = imp.to_string();

                    std::vector<fs::path> candidates;
                    candidates.push_back(parent_dir / (dep_name + ".bin"));
                    for (const auto &root : search_paths) {
                        candidates.push_back(root / (dep_name + ".bin"));
                    }

                    bool found = false;
                    for (const auto &candidate : candidates) {
                        if (fs::exists(candidate)) {
                            if (!load_module_recursive(candidate, search_paths, already_loaded)) {
                                return false;
                            }
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        lg::error("Carbon: '{}' imports '{}' but no .bin found",
                                  bin_path.filename().string(), dep_name);
                        return false;
                    }
                }
            } catch (const std::exception &e) {
                lg::error("Carbon: cannot parse '{}': {}", dci_path.string(), e.what());
                return false;
            }
        }

        // 2. Load the .bin itself.
        return load_module(bin_path);
    }

    void Carbon::unload_module(const std::string &logical_name) {
        ModuleRegistry::Entry *entry = m_impl->modules->find(logical_name);
        if (!entry) {
            lg::warn("Carbon::unload_module: '{}' not loaded", logical_name);
            return;
        }

        // 1. Best-effort on_unload hook.
        //    Contract: a module may define a top-level function named
        //    "<logical_name>::on_unload". We look it up and call it.
        //    If it throws or is missing, we continue.
        const std::string on_unload_name = logical_name + "::on_unload";
        if (void *fn = m_impl->globals->find_symbol_ptr(StringId(on_unload_name))) {
            try {
                auto *lambda = reinterpret_cast<ScriptLambda *>(fn);
                m_impl->vm->execute_function(lambda, RunMode::Run);
                m_impl->vm->purge();
            } catch (const std::exception &e) {
                lg::warn("Carbon: '{}' on_unload threw: {}", logical_name, e.what());
            }
        }

        // 2. Drop every symbol the module contributed.
        for (sid64 sid : entry->symbol_sids) { m_impl->globals->remove_symbol(StringId(sid)); }

        // 3. Drop every SsType the module contributed.
        //    (TypeRegistry does not yet have a remove() method; see the
        //    patch notes below. For now we leave the types in place —
        //    they are harmless and will be overwritten if the module
        //    is reloaded.)
        //
        // for (sid64 sid : entry->ss_type_sids) {
        //     gc::type_registry().remove(sid);
        // }

        // 4. Drop every process whose root frame belongs to the module.
        //    Not implemented yet — see Scheduler.
        //    m_impl->scheduler->kill_all_of_module(logical_name);

        // 5. Release the BinaryFile.
        m_impl->modules->erase(logical_name);

        lg::info("Carbon: unloaded '{}'", logical_name);
    }

    bool Carbon::reload_module(const std::filesystem::path &path) {
        // Find the module by path, unload it, load it again.
        ModuleRegistry::Entry *entry = m_impl->modules->find_by_path(path);
        const std::string      logical_name = entry ? entry->logical_name : path.stem().string();

        unload_module(logical_name);
        return load_module(path);
    }

    bool Carbon::has_module(const std::string &logical_name) const {
        return m_impl->modules->contains(logical_name);
    }

    // ------------------------------------------------------------------------
    // Calling
    // ------------------------------------------------------------------------

    CallResult Carbon::call(const std::string &name, const std::vector<Variant> &args) {
        CallResult out;

        if (!m_impl->initialized) {
            out.error = "Carbon::call: not initialized";
            return out;
        }

        void *fn = m_impl->globals->find_symbol_ptr(StringId(name));
        if (!fn) {
            out.error = "function '" + name + "' not found";
            return out;
        }

        auto *lambda = reinterpret_cast<ScriptLambda *>(fn);

        try {
            out.value = m_impl->vm->execute_function(lambda, RunMode::Run, args);
            m_impl->vm->purge();
            out.ok = m_impl->vm->is_error == false;
            if (!out.ok) out.error = m_impl->vm->break_reason;
        } catch (const std::exception &e) {
            out.ok = false;
            out.error = e.what();
        }

        return out;
    }

    // ------------------------------------------------------------------------
    // Processes (stubs)
    // ------------------------------------------------------------------------

    u64 Carbon::spawn_process(const std::string &entry, const std::vector<Variant> &args) {
        return m_impl->scheduler->spawn(entry, args);
    }

    void Carbon::kill_process(u64 process_id) { m_impl->scheduler->kill(process_id); }

    void Carbon::tick() { m_impl->scheduler->tick(); }

    bool Carbon::should_exit() const noexcept { return m_impl->exit_requested; }

    void Carbon::request_exit() { m_impl->exit_requested = true; }

    // ------------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------------

    VirtualMachine   *Carbon::vm() noexcept { return m_impl->vm.get(); }
    Globals          *Carbon::globals() noexcept { return m_impl->globals.get(); }
    ModuleRegistry   *Carbon::modules() noexcept { return m_impl->modules.get(); }
    Scheduler        *Carbon::scheduler() noexcept { return m_impl->scheduler.get(); }
    gc::ArenaManager *Carbon::gc_heap() noexcept { return m_impl->gc_heap.get(); }

        void Carbon::register_builtin_gc_types() {
        auto &reg = gc::type_registry();

        auto reg_one = [&](const char *name, std::size_t size, std::size_t align, bool is_ref) {
            const sid64 sid = static_cast<sid64>(SID(name));
            if (reg.find(sid)) return;

            gc::TypeDescriptor td{};
            td.name = sid;
            td.size = static_cast<u32>(size);
            td.align = static_cast<u32>(align);
            td.num_ptr_fields = 0;
            td.flags = is_ref ? gc::TYPE_IS_REFERENCE : gc::TYPE_IS_SCALAR;
            td.ptr_fields = nullptr;
            td.mark = nullptr;
            td.destroy = nullptr;
            reg.register_type(td);
        };

        // Fixed-size scalar types.
        reg_one("i64", sizeof(i64), alignof(i64), false);
        reg_one("f32", sizeof(f32), alignof(f32), false);
        reg_one("sid64", sizeof(sid64), alignof(sid64), false);

        // cstr: variable-size raw byte buffer. size is passed to
        // gc::alloc explicitly, so descriptor size is 0. No pointers
        // inside, so no mark delegate.
        reg_one("cstr", 0, 1, false);

        // vector/cvector: not registered here. sootc registers them
        // when generated/vector.hpp is available.
    }

        void Carbon::register_ss_type_in_gc(const SsType &st) {
        auto &reg = gc::type_registry();
        if (reg.find(st.m_name)) return;

        // SsType comes from a loaded file. Its field layout is described
        // by SsField[], but the new TypeDescriptor does not carry a
        // generic field array. Instead, each type provides a mark
        // delegate that walks its own pointers.
        //
        // Generating such a delegate from SsField[] at load time is not
        // possible without emitting code: mark must be a plain C function
        // pointer. So for now we register the type with no traversal.
        //
        // Consequence: any GC pointers inside an SsType instance are not
        // marked and will be reclaimed. This is acceptable while SsType
        // instances do not contain GC references. When they do, this
        // function needs to be replaced with a mechanism that produces a
        // per-type mark function from the layout (e.g. code generation in
        // sootc, or a small interpreter that reads SsField[] and calls
        // the visitor).
        //
        // The `ptr_fields` array is filled in for informational purposes
        // (offsets of pointer-shaped SsFields), but the GC does not use
        // it yet.
        std::vector<gc::PtrField> ptr_fields;
        ptr_fields.reserve(st.m_numFields);

        for (u32 i = 0; i < st.m_numFields; ++i) {
            const SsField &f = st.m_pFields[i];

            const bool is_inline = (f.m_flags & 0x1) != 0;
            const bool is_dynamic = (f.m_flags & 0x2) != 0;
            const bool is_array = (f.m_flags & 0x4) != 0;

            // Skip inline structs, dynamic arrays, and inline arrays.
            // Only single non-inline pointer fields are captured here.
            if (is_inline || is_dynamic || is_array) continue;

            gc::PtrField pf{};
            pf.type_index = reg.find_index(f.m_type);
            pf.offset = f.m_offset;
            pf.array_count = 0;
            pf.flags = 0;
            ptr_fields.push_back(pf);
        }

        gc::TypeDescriptor td{};
        td.name = static_cast<u32>(st.m_name);
        td.size = st.m_size;
        td.align = st.m_align ? st.m_align : 16;
        td.num_ptr_fields = static_cast<u32>(ptr_fields.size());
        td.flags = gc::TYPE_IS_REFERENCE;
        td.ptr_fields = ptr_fields.empty() ? nullptr : ptr_fields.data();
        td.mark = nullptr; // see comment above
        td.destroy = nullptr;
        reg.register_type(td);
    }
} // namespace carbon