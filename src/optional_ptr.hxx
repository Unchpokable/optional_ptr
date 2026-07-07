#pragma once

#ifndef OPTIONAL_PTR_HXX
#define OPTIONAL_PTR_HXX

#include <compare>
#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>

namespace optr
{
namespace detail
{
template<typename T, template<typename...> class U>
struct is_specialization_of : std::false_type {};

template<typename... Args, template<typename...> class U>
struct is_specialization_of<U<Args...>, U> : std::true_type {};

template<typename T, template<typename...> class U>
inline constexpr bool is_specialization_of_v = is_specialization_of<T, U>::value;
} // namespace detail

class bad_optional_ptr_access : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Null pointer dereference";
    }
};

template<typename T>
requires(std::is_object_v<T>)
class optional_ptr final {
public:
    using pointer_type = T*;
    using object_type = T;

    constexpr optional_ptr() noexcept = default;

    constexpr optional_ptr(pointer_type ptr) noexcept : m_ptr(ptr)
    {
    }

    template<typename U>
    requires(std::is_convertible_v<U*, pointer_type>)
    constexpr optional_ptr(U* ptr) noexcept : m_ptr(ptr)
    {
    }

    template<typename U>
    requires(std::is_convertible_v<U*, pointer_type>)
    constexpr optional_ptr(const optional_ptr<U>& other) noexcept : m_ptr(other.get())
    {
    }

    template<typename U>
    requires(std::is_convertible_v<U*, pointer_type>)
    constexpr optional_ptr(const std::shared_ptr<U>& ptr) noexcept : m_ptr(ptr.get())
    {
    }

    template<typename U>
    requires(std::is_convertible_v<U*, pointer_type>)
    constexpr optional_ptr(const std::unique_ptr<U>& ptr) noexcept : m_ptr(ptr.get())
    {
    }

    // Rvalue smart pointers own the pointee and are about to be destroyed at
    // the end of the full expression, so observing their address would leave
    // m_ptr dangling immediately. Deleting these (rather than only the exact
    // shared_ptr<T>/unique_ptr<T> overloads) is required: without the
    // template, an rvalue shared_ptr<U>/unique_ptr<U> for a convertible but
    // different U would bind to the `const std::shared_ptr<U>&`/
    // `const std::unique_ptr<U>&` constructors above instead, silently
    // producing a dangling pointer.
    template<typename U>
    requires(std::is_convertible_v<U*, pointer_type>)
    optional_ptr(std::shared_ptr<U>&&) = delete;

    template<typename U>
    requires(std::is_convertible_v<U*, pointer_type>)
    optional_ptr(std::unique_ptr<U>&&) = delete;

    constexpr void swap(optional_ptr& other) noexcept
    {
        std::swap(m_ptr, other.m_ptr);
    }

    constexpr void swap(pointer_type& other) noexcept
    {
        std::swap(m_ptr, other);
    }

    // classic smart pointer-like interfaces =========

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return m_ptr != nullptr;
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr object_type& value() const
    {
        if(!m_ptr) {
            throw bad_optional_ptr_access();
        }

        return *m_ptr;
    }

    [[nodiscard]] constexpr object_type& value_or(object_type& default_value) const noexcept
    {
        if(!m_ptr) {
            return default_value;
        }

        return *m_ptr;
    }

    [[nodiscard]] constexpr pointer_type get() const noexcept
    {
        return m_ptr;
    }

    [[nodiscard]] constexpr object_type& operator*() const
    {
        return value();
    }

    [[nodiscard]] constexpr pointer_type operator->() const
    {
        if(!m_ptr) {
            throw bad_optional_ptr_access();
        }
        return m_ptr;
    }

    template<typename U>
    [[nodiscard]] constexpr bool operator==(const optional_ptr<U>& other) const
    {
        return static_cast<void*>(m_ptr) == static_cast<void*>(other.get());
    }

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    // Comparison operators are defined ONLY FOR USING WITH std::set\std::map. Result of comparison of optional_ptr pointing to independent
    // objects are not specified by C++ standard. Use with caution.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const optional_ptr& other) const
    {
        if(m_ptr == other.m_ptr) {
            return std::strong_ordering::equal;
        };

        return std::less<pointer_type> {}(m_ptr, other.m_ptr) ? std::strong_ordering::less : std::strong_ordering::greater;
    }

    [[nodiscard]] constexpr std::strong_ordering operator<=>(std::nullptr_t) const
    {
        // The built-in operator<=> candidates only cover pointer <=> pointer
        // of the same type; std::nullptr_t is not one of them, so it must be
        // cast to pointer_type first (the same reason std::unique_ptr/
        // shared_ptr define this overload explicitly instead of relying on
        // the raw operator).
        return m_ptr <=> static_cast<pointer_type>(nullptr);
    }

    // Optional-like interfaces =========

    template<typename F>
    requires(std::invocable<F, pointer_type>)
    constexpr auto and_then(F&& f) const
    {
        using result_type = std::invoke_result_t<F, pointer_type>;

        static_assert(detail::is_specialization_of_v<std::remove_cvref_t<result_type>, optional_ptr>,
            "optional_ptr<T>::and_then(F) requires the return type of F to be a specialization of optional_ptr");

        if(has_value()) {
            return std::invoke(std::forward<F>(f), m_ptr);
        }
        else {
            return std::remove_cvref_t<result_type>(nullptr);
        }
    }

    template<typename F>
    requires(std::invocable<F>)
    constexpr auto or_else(F&& f) const
    {
        using result_type = std::invoke_result_t<F>;

        static_assert(std::is_same_v<std::remove_cvref_t<result_type>, optional_ptr>,
            "optional_ptr<T>::or_else(F) requires the return type of F to be a optional_ptr");

        if(has_value()) {
            return optional_ptr(m_ptr);
        }
        else {
            return std::invoke(std::forward<F>(f));
        }
    }

    template<typename F>
    requires(std::invocable<F, pointer_type>)
    constexpr auto transform(F&& f) const
    {
        using result_type = std::invoke_result_t<F, pointer_type>;

        // result_type is what F returns -- already a pointer (e.g. Payload*).
        // optional_ptr<X> wraps X*, so wrapping optional_ptr<result_type>
        // would wrongly demand a Payload** here; unwrap one level of pointer
        // to get optional_ptr<Payload> instead.
        static_assert(std::is_pointer_v<result_type>, "optional_ptr<T>::transform(F) requires the return type of F to be a pointer type");

        using pointee_type = std::remove_pointer_t<result_type>;

        if(has_value()) {
            return optional_ptr<pointee_type>(std::invoke(std::forward<F>(f), m_ptr));
        }
        else {
            return optional_ptr<pointee_type>();
        }
    }

private:
    pointer_type m_ptr { nullptr };
};
} // namespace optr

template<typename T>
struct std::hash<optr::optional_ptr<T>> {
    size_t operator()(const optr::optional_ptr<T>& opt) const noexcept
    {
        return std::hash<typename optr::optional_ptr<T>::pointer_type> {}(opt.get());
    }
};

namespace optr
{
template<typename T>
constexpr void swap(optional_ptr<T>& lhs, optional_ptr<T>& rhs) noexcept
{
    lhs.swap(rhs);
}
} // namespace optr

#endif
