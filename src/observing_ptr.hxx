#pragma once

#ifndef OBSERVING_PTR_HXX
#define OBSERVING_PTR_HXX

#include <cstddef>
#include <exception>
#include <memory>
#include <type_traits>

class nullderef_exception : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Null pointer dereference";
    }
};

template<typename T>
requires(std::is_object_v<T>)
class observing_ptr final {
public:
    using pointer_type = T*;
    using object_type = T;

    observing_ptr(std::nullptr_t) = delete;

    observing_ptr(pointer_type ptr) noexcept : m_ptr(ptr)
    {
    }

    observing_ptr(const std::shared_ptr<object_type>& ptr) noexcept : m_ptr(ptr.get())
    {
    }

    observing_ptr(const std::unique_ptr<object_type>& ptr) noexcept : m_ptr(ptr.get())
    {
    }

    bool has_value() const noexcept
    {
        return m_ptr != nullptr;
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    object_type& value() const
    {
        if(!m_ptr) {
            throw nullderef_exception();
        }

        return *m_ptr;
    }

    pointer_type get() const noexcept
    {
        return m_ptr;
    }

    object_type& operator*() const
    {
        return value();
    }

    pointer_type operator->() const
    {
        if(!m_ptr) {
            throw nullderef_exception();
        }
        return m_ptr;
    }

private:
    pointer_type m_ptr { nullptr };
};

#endif