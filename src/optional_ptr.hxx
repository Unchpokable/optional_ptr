#pragma once

#ifndef OPTIONAL_PTR_HXX
#define OPTIONAL_PTR_HXX

#include <concepts>
#include <cstddef>
#include <exception>
#include <memory>
#include <type_traits>

namespace detail
{
template<typename T, template<typename...> class U>
struct is_specialization_of : std::false_type {};

template<typename... Args, template<typename...> class U>
struct is_specialization_of<U<Args...>, U> : std::true_type {};

template<typename T, template<typename...> class U>
inline constexpr bool is_specialization_of_v = is_specialization_of<T, U>::value;

template<typename T>
struct always_false : std::false_type {};

template<>
struct always_false<void> : std::false_type {};

template<typename T>
inline constexpr bool always_false_v = always_false<T>::value;

class nullpointer_dereference : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Null pointer dereference";
    }
};
} // namespace detail

template<typename T>
requires(std::is_object_v<T>)
class optional_ptr final {
public:
    using pointer_type = T*;
    using object_type = T;

    optional_ptr(pointer_type ptr) noexcept : m_ptr(ptr)
    {
    }

    optional_ptr(const std::shared_ptr<object_type>& ptr) noexcept : m_ptr(ptr.get())
    {
    }

    optional_ptr(const std::unique_ptr<object_type>& ptr) noexcept : m_ptr(ptr.get())
    {
    }

    optional_ptr(std::shared_ptr<object_type>&&)
    {
        static_assert(detail::always_false_v<T>, "Cannot construct observing_ptr from rvalue shared_ptr");
    };

    optional_ptr(std::unique_ptr<object_type>&&)
    {
        static_assert(detail::always_false_v<T>, "Cannot construct observing_ptr from rvalue unique_ptr");
    };

    // classic smart pointer-like interfaces =========

    constexpr bool has_value() const noexcept
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
            throw detail::nullpointer_dereference();
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
            throw detail::nullpointer_dereference();
        }
        return m_ptr;
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

private:
    optional_ptr(std::nullptr_t)
        : m_ptr(nullptr) {

          };

    pointer_type m_ptr { nullptr };
};

#endif