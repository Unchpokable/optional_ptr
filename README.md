# ObservingPtr

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)

A single-header, non-owning, nullable pointer wrapper for C++20 — `std::optional`'s monadic interface (`and_then`, `or_else`, `transform`), applied to raw pointers instead of values.

```cpp
#include "optional_ptr.hxx"

optr::optional_ptr<Widget> find_widget(int id);

if (auto w = find_widget(42)) {
    w->do_something();
}
```

## Why not just use a raw pointer, or `std::optional<T*>`?

A raw `T*` that might be null works, but it doesn't say so anywhere in the signature — every caller has to remember (or guess) whether `nullptr` is a valid input. `std::optional<T*>` fixes the intent but comes with baggage that doesn't make sense for pointers: it lets you rebind to a *different* `optional<T*>` state that itself owns nothing, and its monadic callbacks (`and_then`, `transform`) are designed to pass around the *pointed-to value*, not the pointer itself.

`optional_ptr<T>` is built specifically for the "this may or may not point to something I don't own" case:

- It's explicit in a signature: `optional_ptr<Widget>` tells every caller null is expected, no comment needed.
- It never manages lifetime — no accidental ownership, no double-free, no surprises. It observes `T*`, `std::shared_ptr<T>`, or `std::unique_ptr<T>` alike, without affecting how long the pointee lives.
- Its callbacks (`and_then`, `or_else`, `transform`) work with pointers throughout, so you can chain pointer-returning lookups without a single manual null check.
- It guards against the pointer-wrapper footgun by construction: you cannot build one from a temporary `shared_ptr`/`unique_ptr` rvalue — that's a compile error, not a dangling pointer at runtime.

## Quick tour

### Basic use

```cpp
Widget* raw = find_widget_or_null();
optr::optional_ptr<Widget> maybe(raw);

if (maybe.has_value()) {
    maybe->update();
}

// or, more idiomatically:
if (maybe) {
    maybe->update();
}
```

### Chained, null-safe traversal with `and_then`

Instead of a staircase of null checks, chain lookups that each return an `optional_ptr`:

```cpp
struct Node { Node* next = nullptr; /* ... */ };

optr::optional_ptr<Node> head(get_head());

auto third = head
    .and_then([](Node* n) { return optr::optional_ptr<Node>(n->next); })
    .and_then([](Node* n) { return optr::optional_ptr<Node>(n->next); });

if (third) {
    // reached safely -- any missing link along the way short-circuits here
}
```

### Fallbacks with `or_else`

```cpp
auto config = find_user_config(user_id)
    .or_else([] { return optr::optional_ptr<Config>(&default_config); });
```

### Mapping to a different pointee with `transform`

Unlike `and_then` (whose callback must already return an `optional_ptr`), `transform`'s callback just returns a plain pointer and the wrapping happens for you:

```cpp
optr::optional_ptr<Header> header = ...;

optr::optional_ptr<Payload> payload = header.transform([](Header* h) {
    return h->payload; // Payload*, not optional_ptr<Payload>
});
```

### Works with the containers and algorithms you'd expect

```cpp
std::set<optr::optional_ptr<Widget>> seen;      // ordered by address
std::unordered_set<optr::optional_ptr<Widget>> unique; // hashed by address
std::sort(widgets.begin(), widgets.end());       // address order
```

## Installation

`optional_ptr.hxx` is a single, dependency-free header (aside from the standard library) — you can copy [src/optional_ptr.hxx](src/optional_ptr.hxx) straight into your project and `#include` it.

For CMake projects, pull it in with `FetchContent` and link against the provided interface target:

```cmake
include(FetchContent)
FetchContent_Declare(
    ObservingPtr
    GIT_REPOSITORY https://github.com/Unchpokable/ObservingPtr.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ObservingPtr)

target_link_libraries(your_target PRIVATE ObservingPtr::optional_ptr)
```

## API at a glance

| Member | What it does |
|---|---|
| `has_value()`, `explicit operator bool()` | Is there a live pointee? |
| `get()` | Raw pointer, may be null |
| `value()`, `operator*`, `operator->` | Access the pointee; throw `optr::bad_optional_ptr_access` if null |
| `value_or(fallback)` | Reference to the pointee, or to `fallback` if null |
| `and_then(f)` | Chain a lookup that itself returns an `optional_ptr` |
| `or_else(f)` | Supply a fallback `optional_ptr` when null |
| `transform(f)` | Map to a different pointee type via a plain pointer-returning callback |
| `swap`, `==`, `<=>`, `std::hash` | Regular-type ergonomics: swappable, comparable, usable as a key |

It converts implicitly from `T*`, `std::shared_ptr<T>`, `std::unique_ptr<T>` (and from pointers/smart pointers to types convertible to `T*`, e.g. a derived class), so you can generally just pass what you already have.

## What this is *not*

- Not an owner. It never extends anyone's lifetime — if the object it observes is destroyed, the `optional_ptr` is left dangling, exactly like a raw pointer would be.
- Not thread-safe on its own — concurrent access to the same instance needs the same external synchronization a raw pointer would.
- Ordering (`<`, `<=>`, `std::set`/`std::map` support) compares by *address*, not by any notion of the pointee's value — it exists so the type can be used as a container key, not to express any domain-meaningful order.

## Requirements

A C++20 compiler with `<concepts>` and `<compare>` support.

## Building & running the tests

```bash
./scripts/build.sh Debug   # or Release
```

This configures with Ninja, builds, and runs the test suite (GoogleTest, fetched automatically via CMake).

## License

[MIT](LICENSE)
