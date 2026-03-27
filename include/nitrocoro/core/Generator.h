/**
 * @file Generator.h
 * @brief Synchronous generator (Generator<T>) and async generator (AsyncGenerator<T>)
 */
#pragma once

#include <coroutine>
#include <exception>
#include <iterator>
#include <optional>

namespace nitrocoro
{

template <typename T>
class [[nodiscard]] Generator
{
public:
    struct promise_type
    {
        T * value_ = nullptr;
        std::exception_ptr exception_;

        Generator get_return_object() noexcept
        {
            return Generator{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T & value) noexcept
        {
            value_ = std::addressof(value);
            return {};
        }

        std::suspend_always yield_value(T && value) noexcept
        {
            value_ = std::addressof(value);
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept { exception_ = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        iterator() noexcept = default;

        explicit iterator(handle_type handle) noexcept
            : handle_(handle) {}

        iterator & operator++()
        {
            handle_.resume();
            if (handle_.done())
            {
                if (handle_.promise().exception_)
                    std::rethrow_exception(handle_.promise().exception_);
                handle_ = nullptr;
            }
            return *this;
        }

        void operator++(int) { ++(*this); }

        reference operator*() const noexcept { return *handle_.promise().value_; }

        pointer operator->() const noexcept { return handle_.promise().value_; }

        bool operator==(const iterator & other) const noexcept { return handle_ == other.handle_; }

        bool operator!=(const iterator & other) const noexcept { return !(*this == other); }

    private:
        handle_type handle_;
    };

    Generator() noexcept = default;

    explicit Generator(handle_type handle) noexcept
        : handle_(handle) {}

    Generator(Generator && other) noexcept
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    Generator & operator=(Generator && other) noexcept
    {
        if (this != &other)
        {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Generator(const Generator &) = delete;
    Generator & operator=(const Generator &) = delete;

    ~Generator()
    {
        if (handle_)
            handle_.destroy();
    }

    iterator begin()
    {
        if (handle_)
        {
            handle_.resume();
            if (handle_.done())
                return iterator{};

            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
        }
        return iterator{ handle_ };
    }

    iterator end() noexcept { return iterator{}; }

private:
    handle_type handle_;
};

template <typename T>
class [[nodiscard]] AsyncGenerator
{
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type
    {
        T * value_ = nullptr;
        std::exception_ptr exception_;
        std::coroutine_handle<> consumer_;

        AsyncGenerator get_return_object() noexcept
        {
            return AsyncGenerator{ handle_type::from_promise(*this) };
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct YieldAwaiter
        {
            std::coroutine_handle<> consumer_;

            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type) noexcept { return consumer_; }
            void await_resume() noexcept {}
        };

        YieldAwaiter yield_value(T & value) noexcept
        {
            value_ = std::addressof(value);
            return YieldAwaiter{ consumer_ };
        }

        YieldAwaiter yield_value(T && value) noexcept
        {
            value_ = std::addressof(value);
            return YieldAwaiter{ consumer_ };
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept { exception_ = std::current_exception(); }

        struct FinalAwaiter
        {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept
            {
                return h.promise().consumer_;
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
    };

    struct [[nodiscard]] NextAwaiter
    {
        handle_type handle_;

        bool await_ready() noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> consumer) noexcept
        {
            handle_.promise().consumer_ = consumer;
            return handle_;
        }

        std::optional<T> await_resume()
        {
            auto & p = handle_.promise();
            if (p.exception_)
                std::rethrow_exception(p.exception_);
            if (handle_.done())
                return std::nullopt;
            return std::optional<T>{ std::move(*p.value_) };
        }
    };

    AsyncGenerator() noexcept = default;

    explicit AsyncGenerator(handle_type h) noexcept
        : handle_(h) {}

    AsyncGenerator(AsyncGenerator && other) noexcept
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    AsyncGenerator & operator=(AsyncGenerator && other) noexcept
    {
        if (this != &other)
        {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    AsyncGenerator(const AsyncGenerator &) = delete;
    AsyncGenerator & operator=(const AsyncGenerator &) = delete;

    ~AsyncGenerator()
    {
        if (handle_)
            handle_.destroy();
    }

    NextAwaiter next() noexcept { return NextAwaiter{ handle_ }; }

private:
    handle_type handle_;
};

} // namespace nitrocoro
