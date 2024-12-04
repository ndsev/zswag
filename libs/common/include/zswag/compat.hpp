#pragma once

#include <string>
#include <utility>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace zswag {
namespace compat {

#if __cplusplus >= 201703L
    #include <optional>
    #include <string_view>
    #include <variant>
    using string_view = std::string_view;
    template<typename T>
    using optional = std::optional<T>;
    template<typename... Types>
    using variant = std::variant<Types...>;
#else
    // Simple string_view implementation for C++14
    class string_view {
        const char* data_;
        size_t size_;
    
    public:
        string_view() noexcept : data_(nullptr), size_(0) {}
        
        string_view(const std::string& str) noexcept 
            : data_(str.data()), size_(str.size()) {}
        
        string_view(const char* str, size_t len) noexcept 
            : data_(str), size_(len) {}

        string_view(const char* str) noexcept
            : data_(str), size_(str ? std::strlen(str) : 0) {}
        
        const char* data() const noexcept { return data_; }
        size_t size() const noexcept { return size_; }
        bool empty() const noexcept { return size_ == 0; }
        
        string_view substr(size_t pos, size_t len) const {
            if (pos > size_)
                throw std::out_of_range("pos > size");
            return string_view(data_ + pos, std::min(len, size_ - pos));
        }
        
        operator std::string() const {
            return std::string(data_, size_);
        }

        bool operator==(const string_view& rhs) const noexcept {
            return size_ == rhs.size_ &&
                   (data_ == rhs.data_ || std::memcmp(data_, rhs.data_, size_) == 0);
        }

        bool operator==(const std::string& rhs) const noexcept {
            return size_ == rhs.size() &&
                   (data_ == rhs.data() || std::memcmp(data_, rhs.data(), size_) == 0);
        }

        bool operator==(const char* str) const noexcept {
            if (!str) return size_ == 0;
            size_t str_len = std::strlen(str);
            return size_ == str_len && std::memcmp(data_, str, size_) == 0;
        }
    };

    inline bool operator==(const char* str, const string_view& rhs) noexcept {
        return rhs == str;
    }

    inline bool operator==(const std::string& str, const string_view& rhs) noexcept {
        return rhs == str;
    }

    // Simple optional implementation for C++14
    template<typename T>
    class optional {
        bool has_value_;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;

    public:
        using value_type = T;

        optional() noexcept : has_value_(false) {}
        
        optional(const T& value) : has_value_(true) {
            new (&storage_) T(value);
        }
        
        optional(T&& value) : has_value_(true) {
            new (&storage_) T(std::move(value));
        }

        optional(const optional& other) : has_value_(other.has_value_) {
            if (has_value_) {
                new (&storage_) T(*other);
            }
        }

        optional(optional&& other) noexcept : has_value_(other.has_value_) {
            if (has_value_) {
                new (&storage_) T(std::move(*other));
            }
            other.has_value_ = false;
        }
        
        ~optional() {
            if (has_value_) {
                reinterpret_cast<T*>(&storage_)->~T();
            }
        }

        optional& operator=(const optional& other) {
            if (this != &other) {
                if (has_value_) {
                    reinterpret_cast<T*>(&storage_)->~T();
                }
                has_value_ = other.has_value_;
                if (has_value_) {
                    new (&storage_) T(*other);
                }
            }
            return *this;
        }

        optional& operator=(optional&& other) noexcept {
            if (this != &other) {
                if (has_value_) {
                    reinterpret_cast<T*>(&storage_)->~T();
                }
                has_value_ = other.has_value_;
                if (has_value_) {
                    new (&storage_) T(std::move(*other));
                }
                other.has_value_ = false;
            }
            return *this;
        }

        bool has_value() const noexcept { return has_value_; }
        operator bool() const noexcept { return has_value_; }

        T& value() {
            if (!has_value_) throw std::runtime_error("bad optional access");
            return *reinterpret_cast<T*>(&storage_);
        }

        const T& value() const {
            if (!has_value_) throw std::runtime_error("bad optional access");
            return *reinterpret_cast<const T*>(&storage_);
        }

        T* operator->() {
            if (!has_value_) throw std::runtime_error("bad optional access");
            return reinterpret_cast<T*>(&storage_);
        }

        const T* operator->() const {
            if (!has_value_) throw std::runtime_error("bad optional access");
            return reinterpret_cast<const T*>(&storage_);
        }

        T& operator*() {
            if (!has_value_) throw std::runtime_error("bad optional access");
            return *reinterpret_cast<T*>(&storage_);
        }

        const T& operator*() const {
            if (!has_value_) throw std::runtime_error("bad optional access");
            return *reinterpret_cast<const T*>(&storage_);
        }
    };

    // Helper for structured bindings replacement in C++14
    template<typename Map>
    class map_entry_helper {
        using iterator = typename Map::const_iterator;
        iterator it_;
        
    public:
        explicit map_entry_helper(iterator it) : it_(it) {}
        
        const typename Map::key_type& key() const { return it_->first; }
        const typename Map::mapped_type& value() const { return it_->second; }
    };

    // Simple variant implementation for C++14
    namespace detail {
        template<typename... Types>
        struct max_size;

        template<>
        struct max_size<> {
            static constexpr size_t value = 0;
        };

        template<typename T, typename... Rest>
        struct max_size<T, Rest...> {
            static constexpr size_t value = sizeof(T) > max_size<Rest...>::value ? 
                sizeof(T) : max_size<Rest...>::value;
        };

        template<typename... Types>
        struct max_align;

        template<>
        struct max_align<> {
            static constexpr size_t value = 0;
        };

        template<typename T, typename... Rest>
        struct max_align<T, Rest...> {
            static constexpr size_t value = alignof(T) > max_align<Rest...>::value ? 
                alignof(T) : max_align<Rest...>::value;
        };

        template<size_t Index, typename T, typename... Types>
        struct type_at;

        template<size_t Index, typename T, typename... Rest>
        struct type_at<Index, T, Rest...> {
            using type = typename type_at<Index - 1, Rest...>::type;
        };

        template<typename T, typename... Rest>
        struct type_at<0, T, Rest...> {
            using type = T;
        };
    }

    template<typename... Types>
    class variant {
        static constexpr size_t data_size = detail::max_size<Types...>::value;
        static constexpr size_t data_align = detail::max_align<Types...>::value;
        
        alignas(data_align) unsigned char data_[data_size];
        size_t index_;

        template<typename T>
        static constexpr size_t index_of() {
            constexpr bool matches[] = { std::is_same<T, Types>::value... };
            for (size_t i = 0; i < sizeof...(Types); ++i) {
                if (matches[i]) return i;
            }
            return static_cast<size_t>(-1);
        }

        template<typename T>
        T* as() {
            return reinterpret_cast<T*>(&data_);
        }

        template<typename T>
        const T* as() const {
            return reinterpret_cast<const T*>(&data_);
        }

        void destroy() {
            visit([](auto& val) { val.~T(); });
        }

    public:
        variant() : index_(0) {
            using T0 = typename detail::type_at<0, Types...>::type;
            new (&data_) T0();
        }

        template<typename T, typename = typename std::enable_if<
            index_of<typename std::decay<T>::type>() != static_cast<size_t>(-1)>::type>
        variant(T&& val) {
            using DecayT = typename std::decay<T>::type;
            index_ = index_of<DecayT>();
            new (&data_) DecayT(std::forward<T>(val));
        }

        ~variant() {
            destroy();
        }

        template<typename F>
        auto visit(F&& f) -> decltype(f(std::declval<typename detail::type_at<0, Types...>::type&>())) {
            using ReturnType = decltype(f(std::declval<typename detail::type_at<0, Types...>::type&>()));
            ReturnType result;
            bool handled = false;

            auto try_visit = [&](auto dummy) {
                using T = decltype(dummy);
                if (index_ == index_of<T>()) {
                    result = f(*as<T>());
                    handled = true;
                }
            };

            int unused[] = { (try_visit(std::declval<Types>()), 0)... };
            (void)unused;

            if (!handled) {
                throw std::runtime_error("Invalid variant access");
            }

            return result;
        }

        size_t index() const { return index_; }
    };
#endif

}
} 