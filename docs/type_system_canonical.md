# TypeSystem Documentation

## Overview

The **TypeSystem** is the core component of the GOAL decompiler and compiler that manages all type information. It provides:

- **Type registration and lookup** — storing and retrieving type definitions
- **Type checking** — verifying type compatibility (`tc`)
- **Method resolution** — looking up methods by name or ID
- **Field access analysis** — computing types and offsets for struct fields
- **Reverse field lookup** — reconstructing field paths from memory access patterns
- **Lowest common ancestor** — computing common parent types
- **Code generation** — generating `deftype` forms from type info

The type system is used by both the compiler (to validate GOAL source) and the decompiler (to reconstruct types from assembly).

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      TypeSystem                         │
│                                                         │
│  m_types: map<name, unique_ptr<Type>>                   │
│  m_forward_declared_types: map<name, parent_name>       │
│  m_forward_declared_method_counts: map<name, count>     │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │ Type (abstract)                                  │   │
│  │  ├── NullType        (none, _type_, _varargs_)   │   │
│  │  ├── ValueType       (int, float, uint8, ...)    │   │
│  │  │    ├── BitFieldType                           │   │
│  │  │    └── EnumType                               │   │
│  │  └── ReferenceType   (pointer semantics)         │   │
│  │       └── StructureType                          │   │
│  │            └── BasicType                         │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### TypeSpec vs Type

- **`TypeSpec`** — a lightweight *reference* to a type or compound type (e.g. `(pointer int)`, `(function int int)`). It's cheap to copy and survives type redefinition.
- **`Type`** — the full definition (fields, methods, size, etc.). Retrieved via `lookup_type()`.

**Rule of thumb:** Store `TypeSpec` whenever possible; only call `lookup_type()` when you need detailed info.

---

## Core Concepts

### ValueType vs ReferenceType

| Aspect | `ValueType` | `ReferenceType` |
|--------|-------------|-----------------|
| Passed by | Value (in register) | Reference (pointer) |
| `is_reference()` | `false` | `true` |
| Examples | `int`, `float`, `uint8` | `object`, `structure`, `basic` |
| `get_load_size()` | Data size | `POINTER_SIZE` (4) |

### Type Hierarchy

Every type (except `object`) has a parent. The type tree determines compatibility:

```
object
├── structure
│   ├── basic          (has runtime type info)
│   │   ├── symbol
│   │   ├── type
│   │   ├── string
│   │   ├── function
│   │   ├── array
│   │   └── ...
│   └── ...
├── pointer
├── inline-array
└── number
    ├── float
    └── integer
        ├── sinteger
        │   ├── int8/16/32/64/128
        └── uinteger
            ├── uint8/16/32/64/128
```

### Compound Types

`TypeSpec` supports arguments for parameterized types:

| TypeSpec | Meaning |
|----------|---------|
| `(pointer int)` | Pointer to `int` |
| `(inline-array my-type)` | Inline array of `my-type` |
| `(array my-type)` | Boxed array of `my-type` |
| `(function int int)` | Function taking `int`, returning `int` |
| `(state my-proc)` | State with `my-proc` as the process type |

---

## Key Operations

### Type Checking — `tc(expected, actual)`

Returns `true` if `actual` is a subtype of (or equal to) `expected`.

```cpp
ts.tc(TypeSpec("integer"), TypeSpec("int8"))  // true (int8 is an integer)
ts.tc(TypeSpec("int8"), TypeSpec("integer"))  // false
ts.tc(TypeSpec("object"), TypeSpec("string")) // true (everything is an object)
```

The more general `typecheck_and_throw()` additionally supports:
- `print_on_error` — log a readable error message
- `throw_on_error` — throw `std::runtime_error` on failure
- `allow_type_alias` — treat `time-frame` as `int` (decompiler uses this)

### Type Lookup

| Method | Behavior on missing type |
|--------|--------------------------|
| `lookup_type()` | Throws |
| `lookup_type_no_throw()` | Returns `nullptr` |
| `lookup_type_allow_partial_def()` | Falls back to forward-declared parent (e.g. `basic`/`structure`) |

### Method Resolution

Methods have IDs and are inherited through the type tree.

```cpp
// Declare a new method (creates it if it doesn't exist)
ts.declare_method(type, "my-method", docstring, no_virtual, function_type, false);

// Define an existing method (must already be declared)
ts.define_method(type, "my-method", function_type, docstring);

// Override a parent method
ts.override_method(type, "my-method", docstring);

// Lookup
MethodInfo info = ts.lookup_method("my-type", "my-method");
MethodInfo info = ts.lookup_method("my-type", 3);  // by ID
```

**Special method:** `"new"` is handled separately — it can be specialized in child types (`add_new_method`).

### Field Access

```cpp
FieldLookupInfo info = ts.lookup_field_info("my-type", "my-field");
// info.field     — the Field
// info.type      — the TypeSpec to use for access
// info.needs_deref — true if a load/store is required
// info.array_size  — size if the field is an array
```

### Deref Information

`get_deref_info(ts)` describes what happens when dereferencing a type:

```cpp
DerefInfo di = ts.get_deref_info(TypeSpec("(pointer int)"));
// di.can_deref   = true
// di.mem_deref   = true    (actually loads from memory)
// di.stride      = 4       (int size)
// di.load_size   = 4
// di.sign_extend = false
// di.result_type = int

DerefInfo di = ts.get_deref_info(TypeSpec("(inline-array my-type)"));
// di.can_deref   = true
// di.mem_deref   = false   (just pointer arithmetic)
// di.stride      = aligned size of my-type
// di.result_type = my-type
```

### Lowest Common Ancestor

```cpp
TypeSpec lca = ts.lowest_common_ancestor(TypeSpec("int8"), TypeSpec("int32"));
// → "integer" (or similar)
```

For compound types, arguments are recursively merged if compatible, otherwise stripped.

### Reverse Field Lookup

The decompiler's most sophisticated feature — given a memory access pattern, reconstruct the field path.

**Input:**
```cpp
FieldReverseLookupInput input;
input.base_type = TypeSpec("(pointer my-struct)");
input.offset = 8;              // byte offset
input.deref = DerefKind{...};  // load/store info (size, sign-extend)
input.stride = 0;              // non-zero for variable indexing
```

**Output:**
```cpp
FieldReverseLookupOutput result = ts.reverse_field_lookup(input);
// result.success     — did we find a match?
// result.addr_of     — is this an address-of operation?
// result.result_type — the type at that location
// result.tokens      — the path (field names, constant indices, variable indices)
```

Tokens can be:
- `FIELD` — a named struct field
- `CONSTANT_IDX` — a constant array index
- `VAR_IDX` — a variable array index (`__VAR__`)

Multiple candidates can be returned via `reverse_field_multi_lookup()`, sorted by score.

---

## Built-in Types

`add_builtin_types(version)` sets up the GOAL runtime type hierarchy:

| Type | Kind | Description |
|------|------|-------------|
| `object` | ValueType | Root of everything |
| `structure` | StructureType | Base for all structures |
| `basic` | BasicType | Has runtime type info |
| `symbol` | BasicType | Named symbol |
| `type` | BasicType | Runtime type object |
| `string` | BasicType | String (final, no virtual calls) |
| `function` | BasicType | Function object |
| `array` | BasicType | Boxed array |
| `pair` | StructureType | Cons cell (offset 2) |
| `pointer` | ValueType | Pointer |
| `inline-array` | ValueType | Inline array |
| `number` | ValueType | Numeric base |
| `float` | ValueType | 4-byte float |
| `integer` | ValueType | 8-byte integer |
| `sinteger`/`uinteger` | ValueType | Signed/unsigned integers |
| `int8`/`int16`/... | ValueType | Sized integers |
| `meters`/`degrees`/`seconds` | ValueType | Unit types |

Game version affects some definitions (e.g. `symbol` is a `basic` in Jak 1, a `structure` with boxed flag in Jak 2+).

---

## Parsing Type Definitions

### `deftype`

Parses a GOAL `deftype` form into a `Type`:

```lisp
(deftype my-type (basic)
  "Optional docstring"
  ((field1 int32)
   (field2 float :offset 8)
   (field3 my-struct :inline)
   (field4 uint8 4)              ; array of 4
   (field5 int32 :dynamic))      ; dynamic array
  (:methods
    (new (symbol type int) _type_)
    (my-method (int) int))
  (:size-assert 32)
  (:method-count-assert 5))
```

Returns `DeftypeResult` with flags, `TypeSpec`, and `Type*`.

Supports:
- `basic`, `structure`, `integer` parents
- `:inline`, `:dynamic`, `:offset`, `:overlay-at`, `:score`, `:decomp-as`, `:offset-assert`, `:do-not-decompile`
- `:methods`, `:states`, `:state-methods`
- `:size-assert`, `:method-count-assert`, `:flag-assert`, `:no-runtime-type`, `:no-inspect`, `:pack-me`, `:heap-base`, `:allow-misaligned`, `:final`, `:always-stack-singleton`

### `defenum`

Parses a GOAL `defenum` form:

```lisp
(defenum my-enum
  "Optional docstring"
  (:type int32)
  (:bitfield #f)
  (entry-a 0)
  (entry-b 1)
  (entry-c))       ; auto-increments
```

Only `integer`-derived base types are supported.

### `parse_typespec`

Converts a `goos::Object` into a `TypeSpec`:

```lisp
int                    → TypeSpec("int")
(pointer int)          → TypeSpec("pointer", {int})
(function int int)     → TypeSpec("function", {int, int})
(function int int :behavior my-proc) → with tag
```

---

## Code Generation

### `generate_deftype(type)`

Regenerates a `deftype` form from a `Type` — useful for the decompiler's type export.

- `generate_deftype_for_structure()` — handles fields, `:overlay-at`, `:offset`, `:inline`, `:dynamic`, arrays
- `generate_deftype_for_bitfield()` — handles bitfields
- `generate_deftype_footer()` — methods, states, flags

The generator attempts to use `:overlay-at` instead of raw `:offset` when it can find a matching field in the same structure (via `find_best_field_in_structure`).

---

## Method Chaining / Inheritance

When a type inherits from a parent:

1. **Fields** are copied from the parent; `m_idx_of_first_unique_field` marks where new fields begin.
2. **Methods** are inherited via `try_lookup_method()` which walks up the parent chain.
3. **States** are stored per-type in `m_states`.

Method IDs are assigned sequentially per type via `get_next_method_id()`. Overridden methods keep the parent's ID.

---

## Common Patterns

### Checking if a type is a subtype

```cpp
if (ts.tc(ts.make_typespec("basic"), some_type)) {
  // some_type is a basic or child of basic
}
```

### Getting the type of a field

```cpp
auto info = ts.lookup_field_info("my-type", "my-field");
TypeSpec field_type = info.type;
```

### Iterating fields

```cpp
auto* st = ts.get_type_of_type<StructureType>("my-type");
for (const auto& field : st->fields()) {
  // field.name(), field.type(), field.offset(), field.is_array(), ...
}
```

### Handling forward-declared types

During parsing, types may not be fully defined yet. Use `lookup_type_allow_partial_def()` which falls back to `basic` or `structure`:

```cpp
Type* t = ts.lookup_type_allow_partial_def("my-type");
```

For loads/stores from partially-defined types, use `get_load_size_allow_partial_def()` (assumes 4 bytes for structures).

---

## Error Handling

The type system uses two error mechanisms:

- **`throw_typesystem_error(msg, args...)`** — prints a colored error message and throws `std::runtime_error`
- **`ASSERT`** — for internal invariants

Errors are raised for:
- Unknown types
- Inconsistent type redefinition
- Invalid method declarations/overrides
- Field offset/size violations
- Invalid `deftype`/`defenum` syntax
- Type check failures (when `throw_on_error` is set)

---

## Summary Table

| Task | API |
|------|-----|
| Look up a type | `lookup_type()`, `lookup_type_no_throw()`, `lookup_type_allow_partial_def()` |
| Check compatibility | `tc(expected, actual)`, `typecheck_and_throw()` |
| Create TypeSpec | `make_typespec()`, `make_pointer_typespec()`, `make_inline_array_typespec()`, `make_function_typespec()` |
| Field info | `lookup_field_info()`, `lookup_field()`, `assert_field_offset()` |
| Method info | `lookup_method()`, `try_lookup_method()`, `declare_method()`, `define_method()`, `override_method()` |
| Deref info | `get_deref_info()` |
| Reverse lookup | `reverse_field_lookup()`, `reverse_field_multi_lookup()` |
| LCA | `lowest_common_ancestor()`, `lowest_common_ancestor_reg()` |
| Parse | `parse_deftype()`, `parse_defenum()`, `parse_typespec()` |
| Generate | `generate_deftype()` |
| Builtins | `add_builtin_types(version)` |
| Search | `search_types_by_parent_type()`, `search_types_by_size()`, `search_types_by_fields()`, etc. |