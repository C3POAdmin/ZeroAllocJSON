#ifndef ZERO_ALLOC_JSON_H
#define ZERO_ALLOC_JSON_H
#include <stddef.h>
#include <stdint.h>
#include <limits>
#include <type_traits>
#include <math.h>

class ZeroAllocJSON {
public:
    enum class Status { Ok, InvalidJSON, Unsupported, DuplicateKey, MissingKey,
                        NullValue, TypeMismatch, OutOfRange, InvalidArgument };
    struct StringView { const char* ptr; size_t length; };
    template<class T> struct Result {
        T value;
        Status status;
        explicit operator bool() const { return status == Status::Ok; }
    };
    ZeroAllocJSON(const char* json, size_t length) : data_(json), size_(length) {}
    Status validate() const;
    Result<StringView> getString(const char* key) const;
    Result<bool> isNull(const char* key) const;
    template<class T> Result<T> get(const char* key) const {
        static_assert(std::is_arithmetic<T>::value, "Use an arithmetic type or getString");
        Token t = {}; Status s = find(key, t);
        if (s != Status::Ok) return Result<T>{T(), s};
        if (t.kind == Kind::Null) return Result<T>{T(), Status::NullValue};
        return convert<T>(t, typename std::is_same<T, bool>::type(),
                          typename std::is_integral<T>::type());
    }
    // Print-compatible sink: write(const uint8_t*, size_t). No Arduino dependency.
    template<class Output> Status showJSON(Output& out) const {
        Status s = validate();
        if (s == Status::Ok) out.write(reinterpret_cast<const uint8_t*>(data_), size_);
        return s;
    }
    template<class Output> Status showVariable(const char* key, Output& out) const {
        Token t = {}; Status s = find(key, t);
        if (s != Status::Ok) return s;
        size_t n = 0; while (key[n]) ++n;
        out.write(reinterpret_cast<const uint8_t*>(key), n);
        const uint8_t separator[] = {':', ' '}; out.write(separator, 2);
        out.write(reinterpret_cast<const uint8_t*>(data_ + t.begin), t.end - t.begin);
        return Status::Ok;
    }
    static const char* statusName(Status status);
private:
    enum class Kind { String, Number, Boolean, Null };
    struct Token { size_t begin, end; Kind kind; };
    const char* data_; size_t size_;
    void space(size_t& p) const;
    Status string(size_t& p, Token& t) const;
    Status value(size_t& p, Token& t) const;
    bool equal(const Token& a, const Token& b) const;
    bool equal(const Token& a, const char* key) const;
    uint32_t character(size_t& p) const;
    Status scan(const char* key, Token* result) const;
    Status find(const char* key, Token& t) const;
    Status magnitude(const Token& t, uint64_t& value, bool& negative) const;
    Status floating(const Token& t, long double& value) const;
    template<class T> Result<T> convert(const Token& t, std::true_type, std::true_type) const {
        if (t.kind != Kind::Boolean) return Result<T>{T(), Status::TypeMismatch};
        return Result<T>{data_[t.begin] == 't', Status::Ok};
    }
    template<class T> Result<T> convert(const Token& t, std::false_type, std::true_type) const {
        uint64_t v = 0; bool neg = false; Status s = magnitude(t, v, neg);
        if (s != Status::Ok) return Result<T>{T(), s};
        const uint64_t max = static_cast<uint64_t>((std::numeric_limits<T>::max)());
        if (neg) {
            if (!std::numeric_limits<T>::is_signed || v > max + uint64_t(1))
                return Result<T>{T(), Status::OutOfRange};
            if (v == max + uint64_t(1)) return Result<T>{(std::numeric_limits<T>::min)(), Status::Ok};
            return Result<T>{static_cast<T>(-static_cast<int64_t>(v)), Status::Ok};
        }
        if (v > max) return Result<T>{T(), Status::OutOfRange};
        return Result<T>{static_cast<T>(v), Status::Ok};
    }
    template<class T> Result<T> convert(const Token& t, std::false_type, std::false_type) const {
        long double v = 0; Status s = floating(t, v);
        if (s != Status::Ok) return Result<T>{T(), s};
        if (v > (std::numeric_limits<T>::max)() || v < -(std::numeric_limits<T>::max)())
            return Result<T>{T(), Status::OutOfRange};
        T result = static_cast<T>(v);
        if (v != 0 && result == 0) return Result<T>{T(), Status::OutOfRange};
        return Result<T>{result, Status::Ok};
    }
};
#endif
