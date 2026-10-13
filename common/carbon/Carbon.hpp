#pragma once

#include "carbon/file/DCScript.hpp"

#include <set>

// Carbon — the runtime.
//
// Carbon owns everything needed to load, link, and execute DC modules:
//
//     - a VirtualMachine         (bytecode execution)
//     - a Globals                (symbol table for THIS context)
//     - a ModuleRegistry         (loaded modules, their imports/exports)
//     - a Scheduler              (processes and their ticks)
//     - a gc::ArenaManager       (heap for THIS context)
//     - a native-function table  (native funcs visible in THIS context)
//
// There is exactly ONE Carbon per execution context. Two Carbons are
// fully isolated: separate heaps, separate symbol tables, separate
// process lists. The only shared global state is StringIdManager
// (SID -> string, one per process) and logging.
//
// Ownership:
//     Carbon owns VirtualMachine, Globals, ModuleRegistry, Scheduler,
//     ArenaManager. It does NOT own BinaryFiles after load_module()
//     returns — they are moved into ModuleRegistry. It does NOT own
//     the native functions themselves (they are static C functions).
//
// Threading:
//     Carbon is NOT thread-safe. All methods must be called from the
//     same thread, or externally synchronised.

#include "CommonTypes.hpp"
#include "carbon/ForwardDeclarations.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <file/BinaryFile.hpp>

namespace carbon {

    class VirtualMachine;
    class Globals;
    class ModuleRegistry;
    class Scheduler;

    namespace gc {
        class ArenaManager;
    }

    /// @brief Configuration for a Carbon instance.
    struct CarbonConfig {
        /// @brief Initial GC heap size in MB. The region is reserved
        ///        immediately; pages are committed lazily.
        std::size_t gc_initial_mb = 16;

        /// @brief Maximum GC heap size in MB.
        std::size_t gc_max_mb = 1024;

        /// @brief If non-empty, load a sidbase.bin from this path at
        ///        initialize() time, so that SID -> string resolution
        ///        works for anything already registered.
        std::string sidbase_path;

        /// @brief If true, register the built-in VM types (i64, f32,
        ///        sid64, vector, cvector) in the GC TypeRegistry.
        ///        Almost always true; the only reason to disable it is
        ///        a test that wants a completely empty TypeRegistry.
        bool register_builtin_types = true;

        /// @brief If true, load the dconstruct stdlib natives (print,
        ///        arithmetic, allocation helpers). Off by default; the
        ///        language runtime owns this decision, not Carbon.
        bool load_default_natives = false;
    };

    /// @brief The result of a top-level call.
    struct CallResult {
        bool        ok = false;
        Variant     value;
        std::string error;
    };

    /// @brief The Carbon runtime.
    ///
    /// Lifecycle:
    ///     Carbon carbon;
    ///     carbon.initialize(config);   // reserve heap, create VM/Globals
    ///     carbon.load_module("foo.bin");
    ///     carbon.call("main-fn", args);
    ///     carbon.shutdown();
    ///
    /// Carbon can also host long-lived processes:
    ///     carbon.spawn_process("worker-fn", args);
    ///     while (!carbon.should_exit()) {
    ///         carbon.tick();
    ///     }
    class Carbon {
    public:

        // --------------------------------------------------------------------
        // Current context
        // --------------------------------------------------------------------
        //
        // Carbon::current() returns the Carbon instance that is currently
        // executing in this thread, or nullptr if none is active.
        //
        // The pointer is set by initialize() and cleared by shutdown() on
        // the same thread. For temporary switches (tests, nested calls),
        // use Carbon::Scope.
        //
        // This API exists ONLY as a migration aid. New code should receive
        // a Carbon& or Carbon* explicitly rather than calling current().
        // Once every call site has been ported, current() and Scope will be
        // removed. Do not build new abstractions on top of it.

        static Carbon *current() noexcept;
        static bool    has_current() noexcept;

        class Scope {
        public:
            explicit Scope(Carbon &c) noexcept;
            ~Scope() noexcept;
            Scope(const Scope &) = delete;
            Scope &operator=(const Scope &) = delete;

        private:
            Carbon *m_previous;
        };


    public:
        Carbon();
        ~Carbon();

        Carbon(const Carbon &) = delete;
        Carbon &operator=(const Carbon &) = delete;

        // ----------------------------------------------------------------
        // Lifecycle
        // ----------------------------------------------------------------

        /// @brief Bring the runtime up. Idempotent: calling it twice is
        ///        a no-op (returns true).
        /// @return true on success.
        bool initialize(const CarbonConfig &config = {});

        /// @brief Bring the runtime down. Drops all modules, all
        ///        processes, releases the GC heap. Idempotent.
        void shutdown();

        /// @return true if initialize() has completed successfully and
        ///         shutdown() has not been called.
        [[nodiscard]] bool is_initialized() const noexcept;

        // ----------------------------------------------------------------
        // Module loading
        // ----------------------------------------------------------------

        /// @brief Load a module from a .bin file.
        ///
        /// Steps:
        ///   1. Read the .bin via BinaryFile::from_path.
        ///   2. Register every SsType entry in the GC TypeRegistry.
        ///   3. Register every named entry in this Carbon's Globals.
        ///   4. Store the BinaryFile in ModuleRegistry under its path.
        ///
        /// The .dci manifest is NOT read here. Import resolution is the
        /// caller's responsibility (see load_module_with_imports below).
        ///
        /// @return true on success.
        bool load_module(const std::filesystem::path &path);
        
         /// @brief Load a module from a deserialized .bin file.
        bool load_module_from_binary(const std::string &logical_name, BinaryFile &&file);

        /// @brief Load a module, then recursively load every import
        ///        listed in its .dci, using search_paths to find them.
        ///
        /// Cyclic imports are handled: a module that is already loaded
        /// (or currently being loaded) is skipped.
        ///
        /// @return true on success.
        bool load_module_with_imports(const std::filesystem::path              &path,
                                      const std::vector<std::filesystem::path> &search_paths = {});

        /// @brief Unload a module by its logical name.
        ///
        /// Contract (stop-world reload, see reload_module for the
        /// full version):
        ///   1. Call <module>::on_unload if defined (best-effort).
        ///   2. Drop every symbol owned by the module from Globals.
        ///   3. Drop every SsType owned by the module from TypeRegistry.
        ///   4. Drop every process whose root frame belongs to the
        ///      module.
        ///   5. Release the BinaryFile.
        ///
        /// Memory owned by the module's data becomes unreachable and
        /// is reclaimed by the next GC pass.
        void unload_module(const std::string &logical_name);

        /// @brief Unload then load again. Semantics are exactly the
        ///        union of unload_module and load_module, in that order.
        bool reload_module(const std::filesystem::path &path);

        /// @return true if a module with this logical name is loaded.
        [[nodiscard]] bool has_module(const std::string &logical_name) const;

        // ----------------------------------------------------------------
        // Calling
        // ----------------------------------------------------------------

        /// @brief Call a top-level function by name. Runs it to
        ///        completion on the current thread, then runs a GC pass.
        ///
        /// This is the "command mode" entry point: fire and forget.
        [[nodiscard]] CallResult call(const std::string          &name,
                                      const std::vector<Variant> &args = {});

        // ----------------------------------------------------------------
        // Processes
        // ----------------------------------------------------------------

        /// @brief Create a process that starts executing `entry` with
        ///        the given arguments. The process is scheduled for the
        ///        next tick().
        ///
        /// Process lifetime rules (see Process.hpp, to be written):
        ///   - A process resolves its ScriptLambda* by SID at call time,
        ///     so reload_module() makes it pick up the new version.
        ///   - A process whose owning module is unloaded is terminated.
        ///   - A process that returns from its entry function exits.
        ///
        /// @return the process id, or 0 on failure.
        u64 spawn_process(const std::string &entry, const std::vector<Variant> &args = {});

        /// @brief Kill a process by id. Runs its exit handler if any.
        void kill_process(u64 process_id);

        /// @brief Advance every live process by one step. This is what
        ///        a game loop or a REPL driver calls.
        ///
        /// Runs a GC pass if the scheduler requests one. Does NOT block.
        void tick();

        /// @return true if a native function has requested a global
        ///         exit (e.g. an exit() native was called from a
        ///         process). The host loop should stop when this
        ///         returns true.
        [[nodiscard]] bool should_exit() const noexcept;

        /// @brief Request that the host loop stop. Called by exit()
        ///        natives and by the REPL's quit command.
        void request_exit();

        // ----------------------------------------------------------------
        // Accessors (for advanced clients and for tests)
        // ----------------------------------------------------------------

        [[nodiscard]] VirtualMachine   *vm() noexcept;
        [[nodiscard]] Globals          *globals() noexcept;
        [[nodiscard]] ModuleRegistry   *modules() noexcept;
        [[nodiscard]] Scheduler        *scheduler() noexcept;

    private:
        bool load_module_recursive(const std::filesystem::path              &bin_path,
                                   const std::vector<std::filesystem::path> &search_paths,
                                   std::set<std::filesystem::path>          &already_loaded);

        void register_builtin_gc_types();
        void register_ss_type_in_gc(const SsType &st);

        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace carbon