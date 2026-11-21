# Code style

Use the repository's clang-format and clang-tidy configurations. Additional rules:

- Use PascalCase for subproject directories, C++ filenames, classes and structs;
  camelCase for functions; snake_case for variables; `m_` for members; and
  CAPS_SNAKE_CASE for constants and enum values. FieldKit's namespace is `FK`.
- Keep each subproject's headers and sources together in its `src/` directory;
  do not introduce a separate `include/` tree.
- Group includes: standard library, platform/third-party libraries, then project
  headers, with one blank line between groups. Leave two blank lines after
  `#pragma once` and after the complete include block.
- Leave one blank line inside each namespace boundary. Group related members
  (construction/destruction, operations, accessors, storage), with two or three
  blank lines between groups and zero or one within a group.
- Separate logical steps inside functions with blank lines: preconditions,
  preparation/work, then return. Avoid dense runs of unrelated statements.
- Use `/** ... */` with Markdown for API documentation. Document contracts and
  surprises, not obvious parameter meanings. Put module overviews before code
  in a `/* ... */` block. Keep comments concise.
- Name APIs for what they do; do not copy standard-library naming by default.
  Keep APIs small and add functionality when needed.
- Make allocators explicit. Document ownership, borrowed lifetimes, invalidation,
  and allocator behavior during moves. Use `NonNullPointer` for rebindable,
  non-null borrowed pointers; it does not own or extend the pointee's lifetime.
- Return `ErrorCodeOr<T>` or `AdvancedErrorOr<T>` for recoverable failures,
  including `void` success. Use `ErrorOr<T, E>` for a custom error type.
- Use debug-only `FK_ASSERT` for low-level caller preconditions such as bounds;
  use `FK_VERIFY` for invariants that must also be checked in release builds.
  Never rely on side effects in debug assertions. Use FK build modes, not NDEBUG.
- Preserve the original error if allocating diagnostic context fails.
- Investigate analyzer warnings before changing the design. Any necessary
  suppression must name the specific check and explain the false positive.
- Keep task reports and design discussions in responses. Do not add READMEs or
  other explanatory documents unless requested.
