// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

module;

#include <auxid/macros.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstring>
#include <format>
#include <functional>
#include <iterator>
#include <new>
#include <optional>
#include <random>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <wyhash/wyhash.h>

#if defined(AUXID_HAS_SSE2) && !defined(AUXID_DISABLE_HASHMAP_SIMD)
#  include <emmintrin.h>
#endif
#if defined(AUXID_HAS_NEON) && !defined(AUXID_DISABLE_HASHMAP_SIMD)
#  include <arm_neon.h>
#endif
#if defined(AUXID_HAS_WASM_SIMD128) && !defined(AUXID_DISABLE_HASHMAP_SIMD)
#  include <wasm_simd128.h>
#endif

export module auxid.containers;

export import auxid.core;
export import auxid.memory;

static_assert(std::endian::native == std::endian::little,
              "Auxid String SSO is designed for Little-Endian architectures.");

export namespace au::containers
{
  template<typename T> using Span = std::span<T>;
  template<typename T1, typename T2> using Pair = std::pair<T1, T2>;
} // namespace au::containers

export namespace au
{
  template<typename T> using Span = std::span<T>;
  template<typename T1, typename T2> using Pair = std::pair<T1, T2>;
} // namespace au

export namespace au
{
#if defined(__cpp_lib_hardware_interference_size)
  inline constexpr usize CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
  inline constexpr usize CACHE_LINE_SIZE = 64;
#endif
} // namespace au

export namespace au
{
  struct StringView;
} // namespace au

namespace au
{
  namespace internal
  {
    inline int compare(const char *s1, const char *s2, usize n)
    {
      if (n == 0)
        return 0;
      return compiler::memcmp(s1, s2, n);
    }

    inline usize length(const char *s)
    {
      return compiler::strlen(s);
    }

    inline const char *find(const char *p, usize n, char c)
    {
      return static_cast<const char *>(compiler::memchr(p, c, n));
    }

    inline usize search_substring(const char *haystack, usize h_len, const char *needle, usize n_len, usize pos)
    {
      if (n_len == 0)
        return pos <= h_len ? pos : static_cast<usize>(-1);
      if (pos >= h_len)
        return static_cast<usize>(-1);
      // A needle longer than the haystack can never match — and h_len - n_len
      // would underflow into a near-SIZE_MAX loop bound (out-of-bounds reads).
      if (n_len > h_len)
        return static_cast<usize>(-1);

      const usize last_possible = h_len - n_len;
      if (pos > last_possible)
        return static_cast<usize>(-1);

      for (usize i = pos; i <= last_possible; ++i)
      {
        if (haystack[i] == needle[0])
        {
          if (compare(haystack + i + 1, needle + 1, n_len - 1) == 0)
          {
            return i;
          }
        }
      }
      return static_cast<usize>(-1);
    }
  } // namespace internal
} // namespace au

export namespace au
{
  struct StringView
  {
    static constexpr usize npos = static_cast<usize>(-1);

    const char *m_ptr = nullptr;
    usize m_len = 0;

public:
    constexpr StringView() = default;

    constexpr StringView(const char *s, usize l) : m_ptr(s), m_len(l)
    {
    }

    constexpr StringView(const char *s) : m_ptr(s), m_len(s ? internal::length(s) : 0)
    {
    }

    constexpr StringView(containers::Span<const char> span) : m_ptr(span.data()), m_len(span.size())
    {
    }

    constexpr StringView(std::string_view sv) : m_ptr(sv.data()), m_len(sv.size())
    {
    }

public:
    using value_type = char;
    using size_type = usize;
    using difference_type = isize;
    using iterator = const char *;
    using const_iterator = const char *;
    using reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr char operator[](usize i) const
    {
      return m_ptr[i];
    }

    [[nodiscard]] constexpr const char *data() const
    {
      return m_ptr;
    }

    [[nodiscard]] constexpr usize size() const
    {
      return m_len;
    }

    [[nodiscard]] constexpr bool empty() const
    {
      return m_len == 0;
    }

    [[nodiscard]] constexpr iterator begin() const noexcept
    {
      return m_ptr;
    }

    [[nodiscard]] constexpr iterator end() const noexcept
    {
      return m_ptr + m_len;
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept
    {
      return begin();
    }

    [[nodiscard]] constexpr const_iterator cend() const noexcept
    {
      return end();
    }

    [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept
    {
      return reverse_iterator(end());
    }

    [[nodiscard]] constexpr reverse_iterator rend() const noexcept
    {
      return reverse_iterator(begin());
    }

    [[nodiscard]] constexpr reverse_iterator crbegin() const noexcept
    {
      return rbegin();
    }

    [[nodiscard]] constexpr reverse_iterator crend() const noexcept
    {
      return rend();
    }

    [[nodiscard]] constexpr char back() const
    {
      return m_ptr[m_len - 1];
    }

    [[nodiscard]] usize find(char c, usize pos = 0) const
    {
      if (pos >= m_len)
        return npos;
      const char *res = internal::find(m_ptr + pos, m_len - pos, c);
      return res ? static_cast<usize>(res - m_ptr) : npos;
    }

    [[nodiscard]] usize find(StringView v, usize pos = 0) const
    {
      return internal::search_substring(m_ptr, m_len, v.data(), v.size(), pos);
    }

    [[nodiscard]] usize find(const char *s, usize pos = 0) const
    {
      return find(StringView(s), pos);
    }

    constexpr bool operator==(StringView other) const
    {
      if (m_len != other.m_len)
        return false;
      return internal::compare(m_ptr, other.m_ptr, m_len) == 0;
    }

    constexpr operator containers::Span<const char>() const
    {
      return containers::Span<const char>(m_ptr, m_len);
    }

    [[nodiscard]] StringView substr(usize pos, usize count = npos) const
    {
      if (pos >= m_len)
        return StringView();
      usize rcount = (count > m_len - pos) ? (m_len - pos) : count;
      return StringView(m_ptr + pos, rcount);
    }

    [[nodiscard]] bool starts_with(StringView prefix) const
    {
      if (prefix.size() > m_len)
        return false;
      return internal::compare(m_ptr, prefix.data(), prefix.size()) == 0;
    }

    [[nodiscard]] bool ends_with(StringView suffix) const
    {
      if (suffix.size() > m_len)
        return false;
      return internal::compare(m_ptr + m_len - suffix.size(), suffix.data(), suffix.size()) == 0;
    }
  };

  u64 hash_string_view(StringView sv)
  {
    return ::wyhash(sv.data(), sv.size(), 0, ::_wyp);
  }
} // namespace au

namespace au::detail
{
  template<typename T>
  [[nodiscard]] constexpr auto format_forward(T &&value) -> decltype(auto)
  {
    using Decay = std::decay_t<T>;
    if constexpr (requires(const Decay &v) {
                     { v.data() } -> std::convertible_to<const char *>;
                     { v.size() } -> std::same_as<usize>;
                   } && !std::is_same_v<Decay, ::au::StringView> && !std::is_same_v<Decay, std::string> &&
                   !std::is_pointer_v<Decay>)
    {
      return ::au::StringView(value.data(), value.size());
    }
    else
    {
      return std::forward<T>(value);
    }
  }
} // namespace au::detail

export namespace au::containers
{
  template<memory::AllocatorType A = memory::HeapAllocator> struct BasicString
  {
    static constexpr usize npos = StringView::npos;

    static constexpr usize SSO_CAPACITY = sizeof(usize) * 3 - 1;

    using value_type = char;
    using size_type = usize;
    using difference_type = isize;
    using reference = char &;
    using const_reference = const char &;
    using pointer = char *;
    using const_pointer = const char *;
    using iterator = char *;
    using const_iterator = const char *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using allocator_type = A;

private:
    struct LongLayout
    {
      usize cap_flagged;
      usize size;
      char *ptr;
    };

    struct ShortLayout
    {
      u8 size_shifted;
      char data[SSO_CAPACITY + 1];
    };

    union {
      LongLayout l;
      ShortLayout s;
    } m_storage;

    AUXID_NO_UNIQUE_ADDRESS A m_allocator;

    [[nodiscard]] bool is_short() const
    {
      return !(m_storage.s.size_shifted & 1);
    }

    [[nodiscard]] usize get_short_size() const
    {
      return m_storage.s.size_shifted >> 1;
    }

    void set_short_size(u8 size)
    {
      m_storage.s.size_shifted = static_cast<u8>(size << 1);
    }

    [[nodiscard]] char *get_data()
    {
      return is_short() ? m_storage.s.data : m_storage.l.ptr;
    }

    [[nodiscard]] const char *get_data() const
    {
      return is_short() ? m_storage.s.data : m_storage.l.ptr;
    }

    [[nodiscard]] usize get_size() const
    {
      return is_short() ? get_short_size() : m_storage.l.size;
    }

    [[nodiscard]] usize get_capacity() const
    {
      return is_short() ? SSO_CAPACITY : (m_storage.l.cap_flagged >> 1);
    }

    void set_long_capacity(usize cap)
    {
      m_storage.l.cap_flagged = (cap << 1) | 1;
    }

public:
    BasicString()
    {
      m_storage.s.size_shifted = 0;
      m_storage.s.data[0] = '\0';
    }

    explicit BasicString(A alloc) : m_allocator(std::move(alloc))
    {
      m_storage.s.size_shifted = 0;
      m_storage.s.data[0] = '\0';
    }

    BasicString(const char *str)
    {
      m_storage.s.size_shifted = 0;
      if (str)
        assign(StringView(str));
      else
        m_storage.s.data[0] = '\0';
    }

    BasicString(const char *str, usize len)
    {
      m_storage.s.size_shifted = 0;

      if (str != nullptr && len > 0)
      {
        assign(StringView(str, len));
      }
      else
      {
        m_storage.s.data[0] = '\0';
      }
    }

    BasicString(StringView sv)
    {
      m_storage.s.size_shifted = 0;
      assign(sv);
    }

    BasicString(StringView sv, A alloc) : m_allocator(std::move(alloc))
    {
      m_storage.s.size_shifted = 0;
      assign(sv);
    }

    BasicString(BasicString &&other) noexcept : m_allocator(static_cast<A &&>(other.m_allocator))
    {
      std::memcpy(&m_storage, &other.m_storage, sizeof(m_storage));

      other.m_storage.s.size_shifted = 0;
      other.m_storage.s.data[0] = '\0';
    }

    BasicString &operator=(BasicString &&other) noexcept
    {
      if (this != &other)
      {
        destroy();
        m_allocator = static_cast<A &&>(other.m_allocator);

        std::memcpy(&m_storage, &other.m_storage, sizeof(m_storage));
        other.m_storage.s.size_shifted = 0;
        other.m_storage.s.data[0] = '\0';
      }
      return *this;
    }

    BasicString(const BasicString &other) : m_allocator(other.m_allocator)
    {
      m_storage.s.size_shifted = 0;
      assign(StringView(other.data(), other.size()));
    }

    BasicString &operator=(const BasicString &other)
    {
      if (this != &other)
      {
        assign(StringView(other.data(), other.size()));
      }
      return *this;
    }

    ~BasicString()
    {
      destroy();
    }

    [[nodiscard]] BasicString clone() const
    {
      BasicString new_str(m_allocator);
      new_str.assign(StringView(get_data(), get_size()));
      return new_str;
    }

    [[nodiscard]] auto get_allocator() const noexcept -> const A &
    {
      return m_allocator;
    }

    void destroy()
    {
      if (!is_short())
      {
        m_allocator.free(m_storage.l.ptr, get_capacity() + 1, 1);
      }
      m_storage.s.size_shifted = 0;
      m_storage.s.data[0] = '\0';
    }

    void reserve(usize new_cap)
    {
      usize current_cap = get_capacity();
      if (new_cap <= current_cap)
        return;

      const usize new_alloc_size = new_cap + 1;

      if (is_short())
      {
        char *new_mem = (char *) m_allocator.alloc(new_alloc_size, 1);

        usize current_len = get_short_size();
        if (current_len > 0)
        {
          std::memcpy(new_mem, m_storage.s.data, current_len);
        }
        new_mem[current_len] = '\0';

        m_storage.l.ptr = new_mem;
        m_storage.l.size = current_len;
        set_long_capacity(new_cap);
      }
      else
      {
        const usize old_alloc_size = current_cap + 1;

        void *expanded = m_allocator.realloc(m_storage.l.ptr, old_alloc_size, new_alloc_size, 1);

        if (expanded)
        {
          m_storage.l.ptr = (char *) expanded;
          set_long_capacity(new_cap);
        }
        else
        {
          char *new_mem = (char *) m_allocator.alloc(new_alloc_size, 1);

          usize current_len = m_storage.l.size;
          std::memcpy(new_mem, m_storage.l.ptr, current_len);
          new_mem[current_len] = '\0';

          m_allocator.free(m_storage.l.ptr, old_alloc_size, 1);

          m_storage.l.ptr = new_mem;
          set_long_capacity(new_cap);
        }
      }
    }

    void assign(StringView sv)
    {
      usize len = sv.size();
      if (len > get_capacity())
      {
        reserve(len);
      }

      if (is_short())
      {
        set_short_size(static_cast<u8>(len));
        if (len > 0)
          std::memcpy(m_storage.s.data, sv.data(), len);
        m_storage.s.data[len] = '\0';
      }
      else
      {
        if (len > 0)
          std::memcpy(m_storage.l.ptr, sv.data(), len);
        m_storage.l.ptr[len] = '\0';
        m_storage.l.size = len;
      }
    }

    void append(StringView sv)
    {
      if (sv.empty())
        return;

      usize cur_len = size();
      usize req_len = cur_len + sv.size();

      if (req_len > get_capacity())
      {
        usize next_cap = get_capacity() * 2;
        if (next_cap < req_len)
          next_cap = req_len;
        reserve(next_cap);
      }

      char *dest = get_data() + cur_len;
      std::memcpy(dest, sv.data(), sv.size());

      if (is_short())
      {
        set_short_size(static_cast<u8>(req_len));
        m_storage.s.data[req_len] = '\0';
      }
      else
      {
        m_storage.l.size = req_len;
        m_storage.l.ptr[req_len] = '\0';
      }
    }

public:
    [[nodiscard]] const char *c_str() const
    {
      return get_data();
    }

    [[nodiscard]] const char *data() const
    {
      return get_data();
    }

    [[nodiscard]] char *data()
    {
      return get_data();
    }

    [[nodiscard]] usize size() const
    {
      return get_size();
    }

    [[nodiscard]] usize length() const
    {
      return get_size();
    }

    [[nodiscard]] bool empty() const
    {
      return get_size() == 0;
    }

public:
    void push(char c)
    {
      char tmp = c;
      append(StringView(&tmp, 1));
    }

    void pop()
    {
      const usize current_size = get_size();

      if (current_size > 0)
      {
        const usize new_size = current_size - 1;

        if (is_short())
        {
          m_storage.s.data[new_size] = '\0';
          set_short_size(static_cast<u8>(new_size));
        }
        else
        {
          m_storage.l.ptr[new_size] = '\0';
          m_storage.l.size = new_size;
        }
      }
    }

    void push_back(char c)
    {
      usize cur_len = get_size();
      if (cur_len >= get_capacity())
      {
        reserve(cur_len == 0 ? 8 : get_capacity() * 2);
      }

      if (is_short())
      {
        m_storage.s.data[cur_len] = c;
        m_storage.s.data[cur_len + 1] = '\0';
        set_short_size(static_cast<u8>(cur_len + 1));
      }
      else
      {
        m_storage.l.ptr[cur_len] = c;
        m_storage.l.ptr[cur_len + 1] = '\0';
        m_storage.l.size = cur_len + 1;
      }
    }

    void pop_back()
    {
      pop();
    }

public:
    char *begin()
    {
      return get_data();
    }

    char *end()
    {
      return get_data() + get_size();
    }

    [[nodiscard]] const char *begin() const
    {
      return get_data();
    }

    [[nodiscard]] const char *end() const
    {
      return get_data() + get_size();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
      return begin();
    }

    [[nodiscard]] const_iterator cend() const noexcept
    {
      return end();
    }

    [[nodiscard]] reverse_iterator rbegin() noexcept
    {
      return reverse_iterator(end());
    }

    [[nodiscard]] reverse_iterator rend() noexcept
    {
      return reverse_iterator(begin());
    }

    [[nodiscard]] const_reverse_iterator rbegin() const noexcept
    {
      return const_reverse_iterator(end());
    }

    [[nodiscard]] const_reverse_iterator rend() const noexcept
    {
      return const_reverse_iterator(begin());
    }

    [[nodiscard]] const_reverse_iterator crbegin() const noexcept
    {
      return rbegin();
    }

    [[nodiscard]] const_reverse_iterator crend() const noexcept
    {
      return rend();
    }

    [[nodiscard]] char &back()
    {
#if !defined(NDEBUG)
      if (get_size() == 0)
        panic("Called back() on empty String");
#endif
      return get_data()[get_size() - 1];
    }

    [[nodiscard]] const char &back() const
    {
#if !defined(NDEBUG)
      if (get_size() == 0)
        panic("Called back() on empty String");
#endif
      return get_data()[get_size() - 1];
    }

    operator Span<char>()
    {
      return Span<char>(get_data(), get_size());
    }

    operator Span<const char>() const
    {
      return Span<const char>(get_data(), get_size());
    }

    operator StringView() const
    {
      return StringView(get_data(), get_size());
    }

    [[nodiscard]] Span<const u8> as_bytes() const
    {
      auto bytes = std::as_bytes(Span<const char>(get_data(), get_size()));
      return Span<const u8>(reinterpret_cast<const u8 *>(bytes.data()), bytes.size());
    }

    [[nodiscard]] usize find(char c, usize pos = 0) const
    {
      return StringView(get_data(), get_size()).find(c, pos);
    }

    [[nodiscard]] usize find(StringView v, usize pos = 0) const
    {
      return StringView(get_data(), get_size()).find(v, pos);
    }

    [[nodiscard]] usize find(const char *s, usize pos = 0) const
    {
      return StringView(get_data(), get_size()).find(StringView(s), pos);
    }

    auto operator+=(StringView other) -> void
    {
      append(other);
    }

public:
    void clear()
    {
      if (is_short())
        set_short_size(0);
      else
        m_storage.l.size = 0;

      get_data()[0] = '\0';
    }

    [[nodiscard]] StringView substr(usize pos, usize count = npos) const
    {
      return StringView(get_data(), get_size()).substr(pos, count);
    }

private:
    // Output iterator appending straight into a BasicString, so vformat never
    // routes through a std::string temporary on the global heap.
    struct AppendIterator
    {
      using iterator_category = std::output_iterator_tag;
      using value_type = void;
      using difference_type = isize;
      using pointer = void;
      using reference = void;

      BasicString *target = nullptr;

      constexpr auto operator*() -> AppendIterator &
      {
        return *this;
      }

      constexpr auto operator++() -> AppendIterator &
      {
        return *this;
      }

      constexpr auto operator++(int) -> AppendIterator
      {
        return *this;
      }

      auto operator=(char c) -> AppendIterator &
      {
        target->append(StringView(&c, 1));
        return *this;
      }
    };

public:
    // Rigid note: fmt is a RUNTIME format string. An invalid one raises
    // std::format_error inside vformat_to, which under -fno-exceptions is a
    // process abort — treat runtime format strings as trusted (format()
    // validates at compile time; prefer it).
    static BasicString vformat(StringView fmt, std::format_args args)
    {
      BasicString out;
      std::vformat_to(AppendIterator{&out}, std::string_view{fmt.data(), fmt.size()}, args);
      return out;
    }

    template<typename... Args> static BasicString format(StringView fmt, Args &&...args)
    {
      auto converted = std::make_tuple(::au::detail::format_forward(args)...);
      return std::apply(
          [&](auto &...conv) { return vformat(fmt, std::make_format_args(conv...)); }, converted);
    }
  };

  using String = BasicString<memory::HeapAllocator>;
} // namespace au::containers

export namespace au
{
  constexpr bool operator==(StringView lhs, const char *rhs)
  {
    return lhs == StringView(rhs);
  }

  constexpr bool operator==(const char *lhs, StringView rhs)
  {
    return StringView(lhs) == rhs;
  }
} // namespace au

export namespace au::containers
{
  template<class A1, class A2> inline bool operator==(const BasicString<A1> &lhs, const BasicString<A2> &rhs)
  {
    if constexpr (std::is_same_v<A1, A2>)
    {
      if (&lhs == &rhs)
        return true;
    }
    if (lhs.size() != rhs.size())
      return false;
    return StringView(lhs.data(), lhs.size()) == StringView(rhs.data(), rhs.size());
  }

  template<class A> inline bool operator==(const BasicString<A> &lhs, StringView rhs)
  {
    return StringView(lhs.data(), lhs.size()) == rhs;
  }

  template<class A> inline bool operator==(StringView lhs, const BasicString<A> &rhs)
  {
    return lhs == StringView(rhs.data(), rhs.size());
  }

  template<class A> inline bool operator==(const BasicString<A> &lhs, const char *rhs)
  {
    return StringView(lhs.data(), lhs.size()) == StringView(rhs);
  }

  template<class A> inline bool operator==(const char *lhs, const BasicString<A> &rhs)
  {
    return StringView(lhs) == StringView(rhs.data(), rhs.size());
  }

  template<class A> inline BasicString<A> operator+(const BasicString<A> &lhs, StringView rhs)
  {
    BasicString<A> result(lhs.get_allocator());
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs);
    result.append(rhs);
    return result;
  }

  template<class A> inline BasicString<A> operator+(StringView lhs, const BasicString<A> &rhs)
  {
    BasicString<A> result(rhs.get_allocator());
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs);
    result.append(rhs);
    return result;
  }

  template<class A> inline BasicString<A> operator+(const BasicString<A> &lhs, const BasicString<A> &rhs)
  {
    BasicString<A> result(lhs.get_allocator());
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs);
    result.append(rhs);
    return result;
  }

  template<class A> inline BasicString<A> operator+(const char *lhs, const BasicString<A> &rhs)
  {
    return StringView(lhs) + rhs;
  }

  template<class A> inline BasicString<A> operator+(const BasicString<A> &lhs, const char *rhs)
  {
    return lhs + StringView(rhs);
  }

  template<class A> inline BasicString<A> operator+(const BasicString<A> &lhs, char rhs)
  {
    BasicString<A> result(lhs.get_allocator());
    result.reserve(lhs.size() + 1);
    result.append(lhs);
    result.push(rhs);
    return result;
  }
} // namespace au::containers

template<> inline constexpr bool std::ranges::enable_borrowed_range<au::StringView> = true;

export namespace au
{
  template<memory::AllocatorType A> using BasicString = containers::BasicString<A>;
  using String = containers::String;
} // namespace au

export namespace au::containers
{
  struct NullOptType
  {
    explicit constexpr NullOptType(int)
    {
    }
  };

  inline constexpr NullOptType nullopt{0};

  template<typename T> class [[nodiscard]] Option
  {
    static_assert(!std::is_reference_v<T>, "Option<T&> is not allowed. Use Option<T*>.");

    union {
      char m_dummy;
      T m_val;
    };

    bool m_has_value;

public:
    constexpr Option() noexcept : m_dummy(0), m_has_value(false)
    {
    }

    constexpr Option(NullOptType) noexcept : m_dummy(0), m_has_value(false)
    {
    }

    constexpr Option(const T &val) : m_val(val), m_has_value(true)
    {
    }

    constexpr Option(T &&val) : m_val(std::move(val)), m_has_value(true)
    {
    }

    constexpr Option(const std::optional<T> &opt) : m_dummy(0), m_has_value(false)
    {
      if (opt.has_value())
      {
        au::construct_at(&m_val, *opt);
        m_has_value = true;
      }
    }

    constexpr Option(std::optional<T> &&opt) : m_dummy(0), m_has_value(false)
    {
      if (opt.has_value())
      {
        au::construct_at(&m_val, std::move(*opt));
        m_has_value = true;
      }
    }

    [[nodiscard]] constexpr operator std::optional<T>() const &
    {
      if (m_has_value)
        return std::optional<T>{m_val};
      return std::nullopt;
    }

    [[nodiscard]] constexpr operator std::optional<T>() &&
    {
      if (m_has_value)
        return std::optional<T>{std::move(m_val)};
      return std::nullopt;
    }

    constexpr Option(const Option &other)
    {
      if (other.m_has_value)
      {
        au::construct_at(&m_val, other.m_val);
        m_has_value = true;
      }
      else
      {
        m_has_value = false;
      }
    }

    constexpr Option(Option &&other) noexcept
    {
      if (other.m_has_value)
      {
        au::construct_at(&m_val, std::move(other.m_val));
        m_has_value = true;
      }
      else
      {
        m_has_value = false;
      }
    }

    constexpr ~Option()
      requires(!std::is_trivially_destructible_v<T>)
    {
      reset();
    }

    constexpr ~Option() = default;

public:
    Option &operator=(NullOptType)
    {
      reset();
      return *this;
    }

    Option &operator=(const Option &other)
    {
      if (this != &other)
      {
        if (other.m_has_value)
        {
          if (m_has_value)
          {
            m_val = other.m_val;
          }
          else
          {
            au::construct_at(&m_val, other.m_val);
            m_has_value = true;
          }
        }
        else
        {
          reset();
        }
      }
      return *this;
    }

    Option &operator=(Option &&other) noexcept
    {
      if (this != &other)
      {
        if (other.m_has_value)
        {
          if (m_has_value)
          {
            m_val = std::move(other.m_val);
          }
          else
          {
            au::construct_at(&m_val, std::move(other.m_val));
            m_has_value = true;
          }
        }
        else
        {
          reset();
        }
      }
      return *this;
    }

public:
    [[nodiscard]] constexpr bool has_value() const
    {
      return m_has_value;
    }

    [[nodiscard]] constexpr bool is_some() const
    {
      return m_has_value;
    }

    [[nodiscard]] constexpr bool is_none() const
    {
      return !m_has_value;
    }

    constexpr explicit operator bool() const
    {
      return m_has_value;
    }

    constexpr T &operator*() &
    {
#if !defined(NDEBUG)
      if (!m_has_value)
        panic("Option::operator* on None");
#endif
      return m_val;
    }

    constexpr const T &operator*() const &
    {
#if !defined(NDEBUG)
      if (!m_has_value)
        panic("Option::operator* on None");
#endif
      return m_val;
    }

    constexpr T *operator->()
    {
#if !defined(NDEBUG)
      if (!m_has_value)
        panic("Option::operator-> on None");
#endif
      return &m_val;
    }

    constexpr const T *operator->() const
    {
#if !defined(NDEBUG)
      if (!m_has_value)
        panic("Option::operator-> on None");
#endif
      return &m_val;
    }

    constexpr T &unwrap(std::source_location loc = std::source_location::current()) &
    {
      if (!m_has_value)
        panic("Called unwrap() on None Option", loc);
      return m_val;
    }

    constexpr const T &unwrap(std::source_location loc = std::source_location::current()) const &
    {
      if (!m_has_value)
        panic("Called unwrap() on None Option", loc);
      return m_val;
    }

    constexpr T unwrap(std::source_location loc = std::source_location::current()) &&
    {
      if (!m_has_value)
        panic("Called unwrap() on None Option", loc);
      return std::move(m_val);
    }

    constexpr T &expect(const char *msg, std::source_location loc = std::source_location::current()) &
    {
      if (!m_has_value)
        panic(msg, loc);
      return m_val;
    }

    template<typename U> constexpr T value_or(U &&def) const &
    {
      return m_has_value ? m_val : static_cast<T>(std::forward<U>(def));
    }

    template<typename U> constexpr T value_or(U &&def) &&
    {
      return m_has_value ? std::move(m_val) : static_cast<T>(std::forward<U>(def));
    }

    template<typename F> constexpr auto map(F &&f) const & -> Option<std::invoke_result_t<F, const T &>>
    {
      if (m_has_value)
      {
        return std::invoke(std::forward<F>(f), m_val);
      }
      return nullopt;
    }

public:
    void reset()
    {
      if (m_has_value)
      {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
          au::destroy_at(&m_val);
        }
        m_has_value = false;
      }
    }
  };
} // namespace au::containers

export namespace au
{
  template<typename T> using Option = containers::Option<T>;
}

export namespace au
{
  // The Auxid error currency (DECISIONS.md D-008). Small, movable, and
  // allocation-free unless a message is attached: an Err(Error::os_last())
  // costs no more than returning an int. The OS Foundation Layer's honest
  // error unit is the raw platform code plus a domain tag (D-002) — a
  // classifier (e.g. fs_kind()) can be layered on later without changing
  // this type.
  enum class ErrorDomain : u16
  {
    Generic, // unclassified failures, incl. bare fail("...")
    Os,      // raw errno / GetLastError from the OS Foundation Layer
    Fs,      // filesystem ops; code is the platform error value
    Net,     // reserved for IANet interop
    Parse,
    App,     // product-defined codes
  };

  class AUXID_API Error
  {
public:
    ErrorDomain domain = ErrorDomain::Generic;
    i32 code = 0;
    Option<String> message{};

    Error() = default;

    Error(ErrorDomain err_domain, i32 err_code) : domain(err_domain), code(err_code)
    {
    }

    Error(ErrorDomain err_domain, i32 err_code, String msg)
        : domain(err_domain), code(err_code), message(std::move(msg))
    {
    }

    // Implicit on purpose: keeps bare fail("...") and fail(String::format(...))
    // working as Generic errors (D-008 ruling: app-level ergonomics matter).
    Error(String msg) : message(std::move(msg))
    {
    }

    Error(StringView msg) : message(String(msg))
    {
    }

    Error(const char *msg) : message(String(msg))
    {
    }

    [[nodiscard]] static auto os(i32 err_code) -> Error
    {
      return Error(ErrorDomain::Os, err_code);
    }

    // Captures errno (POSIX) / GetLastError() (Windows) at the call site.
    [[nodiscard]] static auto os_last() -> Error;

    [[nodiscard]] static auto app(i32 err_code, StringView msg) -> Error
    {
      return Error(ErrorDomain::App, err_code, String(msg));
    }

    // Context chaining, outermost first: open(path).ctx("loading config")
    // describes as "fs/2: loading config: open: ...". Allocates only when
    // called — the plain error path stays allocation-free.
    [[nodiscard]] auto ctx(StringView context) && -> Error
    {
      if (message.has_value())
      {
        String combined{};
        combined.reserve(context.size() + 2 + message->size());
        combined.append(context);
        combined.append(": ");
        combined.append(StringView(message->data(), message->size()));
        message = std::move(combined);
      }
      else
      {
        message = String(context);
      }
      return std::move(*this);
    }

    [[nodiscard]] auto domain_name() const -> const char *
    {
      switch (domain)
      {
      case ErrorDomain::Generic:
        return "generic";
      case ErrorDomain::Os:
        return "os";
      case ErrorDomain::Fs:
        return "fs";
      case ErrorDomain::Net:
        return "net";
      case ErrorDomain::Parse:
        return "parse";
      case ErrorDomain::App:
        return "app";
      }
      return "unknown";
    }

    // "os/2: loading config: open: no such file". A bare Generic message
    // renders as just the message.
    [[nodiscard]] auto describe() const -> String
    {
      if (domain == ErrorDomain::Generic && code == 0 && message.has_value())
        return message->clone();
      if (message.has_value())
        return String::format("{}/{}: {}", domain_name(), code, message->c_str());
      return String::format("{}/{}", domain_name(), code);
    }
  };
}

export namespace au::containers
{
  template<typename T, typename IndexT, typename AllocatorT = memory::HeapAllocator>
    requires memory::AllocatorType<AllocatorT>
  class CompactVecBase
  {
public:
    using value_type = T;
    using size_type = IndexT;
    using difference_type = std::make_signed_t<IndexT>;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr CompactVecBase() noexcept = default;

    explicit CompactVecBase(size_type init_size, const T &init_value = T{})
    {
      resize(init_size, init_value);
    }

    CompactVecBase(std::initializer_list<T> init)
    {
      reserve_exact(static_cast<size_type>(init.size()));
      if constexpr (std::is_trivially_copyable_v<T>)
      {
        if (init.size() > 0)
          std::memcpy(m_data, init.begin(), init.size() * sizeof(T));
        m_size = static_cast<size_type>(init.size());
      }
      else
      {
        for (const auto &item : init)
          push_back(item);
      }
    }

    CompactVecBase(CompactVecBase &&other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity),
          m_allocator(std::move(other.m_allocator))
    {
      other.m_data = nullptr;
      other.m_size = 0;
      other.m_capacity = 0;
    }

    auto operator=(CompactVecBase &&other) noexcept -> CompactVecBase &
    {
      if (this != &other)
      {
        clear();
        if (m_data)
          m_allocator.free(m_data, m_capacity * sizeof(T), alignof(T));
        m_allocator = std::move(other.m_allocator);
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
      }
      return *this;
    }

    CompactVecBase(const CompactVecBase &other)
    {
      reserve_exact(other.m_size);
      if constexpr (std::is_trivially_copyable_v<T>)
      {
        if (other.m_size > 0)
          std::memcpy(m_data, other.m_data, other.m_size * sizeof(T));
        m_size = other.m_size;
      }
      else
      {
        for (size_type i = 0; i < other.m_size; ++i)
          au::construct_at(&m_data[i], other.m_data[i]);
        m_size = other.m_size;
      }
    }

    auto operator=(const CompactVecBase &other) -> CompactVecBase &
    {
      if (this != &other)
      {
        clear();
        reserve_exact(other.m_size);
        if constexpr (std::is_trivially_copyable_v<T>)
        {
          if (other.m_size > 0)
            std::memcpy(m_data, other.m_data, other.m_size * sizeof(T));
          m_size = other.m_size;
        }
        else
        {
          for (size_type i = 0; i < other.m_size; ++i)
            au::construct_at(&m_data[i], other.m_data[i]);
          m_size = other.m_size;
        }
      }
      return *this;
    }

    ~CompactVecBase()
    {
      clear();
      if (m_data)
        m_allocator.free(m_data, m_capacity * sizeof(T), alignof(T));
    }

    template<typename... Args> auto emplace_back(Args &&...args) -> T &
    {
      if (m_size >= m_capacity)
        grow();
      au::construct_at(&m_data[m_size], std::forward<Args>(args)...);
      return m_data[m_size++];
    }

    auto push_back(const T &val) -> void
    {
      emplace_back(val);
    }

    auto push_back(T &&val) -> void
    {
      emplace_back(std::move(val));
    }

    auto push(const T &val) -> void
    {
      emplace_back(val);
    }

    auto push(T &&val) -> void
    {
      emplace_back(std::move(val));
    }

    auto pop_back() -> void
    {
      if (m_size > 0)
      {
        --m_size;
        if constexpr (!std::is_trivially_destructible_v<T>)
          au::destroy_at(&m_data[m_size]);
      }
    }

    auto pop() -> void
    {
      pop_back();
    }

    auto clear() -> void
    {
      if constexpr (!std::is_trivially_destructible_v<T>)
      {
        for (size_type i = 0; i < m_size; ++i)
          au::destroy_at(&m_data[i]);
      }
      m_size = 0;
    }

    auto reserve_exact(size_type new_cap) -> void
    {
      if (new_cap <= m_capacity)
        return;
      reallocate(new_cap);
    }

    auto reserve_at_least(size_type new_cap) -> void
    {
      if (new_cap <= m_capacity)
        return;
      const size_type geom =
          (m_capacity == 0) ? static_cast<size_type>(8) : static_cast<size_type>(m_capacity + (m_capacity / 2) + 1);
      reallocate(geom > new_cap ? geom : new_cap);
    }

    auto reserve(size_type new_cap) -> void
    {
      reserve_at_least(new_cap);
    }

    auto resize(size_type new_size) -> void
    {
      if (new_size > m_size)
      {
        if (new_size > m_capacity)
          reserve_at_least(new_size);
        for (size_type i = m_size; i < new_size; ++i)
          au::construct_at(&m_data[i]);
      }
      else if (new_size < m_size)
      {
        if constexpr (!std::is_trivially_destructible_v<T>)
          for (size_type i = new_size; i < m_size; ++i)
            au::destroy_at(&m_data[i]);
      }
      m_size = new_size;
    }

    auto resize(size_type new_size, const T &fill_val) -> void
    {
      if (new_size > m_size)
      {
        if (new_size > m_capacity)
          reserve_at_least(new_size);
        for (size_type i = m_size; i < new_size; ++i)
          au::construct_at(&m_data[i], fill_val);
      }
      else if (new_size < m_size)
      {
        if constexpr (!std::is_trivially_destructible_v<T>)
          for (size_type i = new_size; i < m_size; ++i)
            au::destroy_at(&m_data[i]);
      }
      m_size = new_size;
    }

    [[nodiscard]] auto size() const noexcept -> size_type
    {
      return m_size;
    }

    [[nodiscard]] auto capacity() const noexcept -> size_type
    {
      return m_capacity;
    }

    [[nodiscard]] auto empty() const noexcept -> bool
    {
      return m_size == 0;
    }

    [[nodiscard]] auto data() noexcept -> T *
    {
      return m_data;
    }

    [[nodiscard]] auto data() const noexcept -> const T *
    {
      return m_data;
    }

    auto operator[](size_type idx) -> T &
    {
#if !defined(NDEBUG)
      if (idx >= m_size)
        panic("CompactVecBase index out of bounds");
#endif
      return m_data[idx];
    }

    auto operator[](size_type idx) const -> const T &
    {
#if !defined(NDEBUG)
      if (idx >= m_size)
        panic("CompactVecBase index out of bounds");
#endif
      return m_data[idx];
    }

    auto begin() noexcept -> T *
    {
      return m_data;
    }

    auto end() noexcept -> T *
    {
      return m_data + m_size;
    }

    auto begin() const noexcept -> const T *
    {
      return m_data;
    }

    auto end() const noexcept -> const T *
    {
      return m_data + m_size;
    }

    auto cbegin() const noexcept -> const_iterator
    {
      return begin();
    }

    auto cend() const noexcept -> const_iterator
    {
      return end();
    }

    auto back() -> T &
    {
      return m_data[m_size - 1];
    }

    auto back() const -> const T &
    {
      return m_data[m_size - 1];
    }

    operator std::span<T>()
    {
      return std::span<T>(m_data, m_size);
    }

    operator std::span<const T>() const
    {
      return std::span<const T>(m_data, m_size);
    }

    [[nodiscard]] auto as_span() -> std::span<T>
    {
      return std::span<T>(m_data, m_size);
    }

    [[nodiscard]] auto as_span() const -> std::span<const T>
    {
      return std::span<const T>(m_data, m_size);
    }

private:
    auto reallocate(size_type new_cap) -> void
    {
      if constexpr (std::is_trivially_copyable_v<T>)
      {
        if (m_data)
        {
          void *ptr = m_allocator.realloc(m_data, m_capacity * sizeof(T), new_cap * sizeof(T), alignof(T));
          m_data = static_cast<T *>(ptr);
          m_capacity = new_cap;
          return;
        }
      }
      T *new_data = static_cast<T *>(m_allocator.alloc(new_cap * sizeof(T), alignof(T)));
      if (m_data)
      {
        if constexpr (std::is_trivially_copyable_v<T>)
        {
          std::memcpy(new_data, m_data, m_size * sizeof(T));
        }
        else
        {
          for (size_type i = 0; i < m_size; ++i)
          {
            au::construct_at(&new_data[i], std::move(m_data[i]));
            au::destroy_at(&m_data[i]);
          }
        }
        m_allocator.free(m_data, m_capacity * sizeof(T), alignof(T));
      }
      m_data = new_data;
      m_capacity = new_cap;
    }

    auto grow() -> void
    {
      reserve_at_least(static_cast<size_type>(m_size + 1));
    }

    T *m_data = nullptr;
    size_type m_size = 0;
    size_type m_capacity = 0;
    AUXID_NO_UNIQUE_ADDRESS AllocatorT m_allocator{};
  };

  template<typename T, typename IndexT = u32, typename AllocatorT = memory::HeapAllocator>
  using VecT = CompactVecBase<T, IndexT, AllocatorT>;
} // namespace au::containers

export namespace au
{
  template<typename T, memory::AllocatorType A = memory::HeapAllocator>
  using Vec = std::vector<T, memory::StdAllocatorAdapter<T, A>>;

  template<typename T, memory::AllocatorType A = memory::HeapAllocator>
  using TinyVec = containers::CompactVecBase<T, u16, A>;

  template<typename T, memory::AllocatorType A = memory::HeapAllocator>
  using CompactVec = containers::CompactVecBase<T, u32, A>;
} // namespace au

export namespace au
{
  template<typename T, typename A> [[nodiscard]] inline auto clone(const std::vector<T, A> &v) -> std::vector<T, A>
  {
    return v;
  }

  template<typename T, typename I, typename A>
  [[nodiscard]] inline auto clone(const containers::CompactVecBase<T, I, A> &v) -> containers::CompactVecBase<T, I, A>
  {
    return v;
  }

  template<typename T, typename A>
  inline auto reserve_exact(std::vector<T, A> &v, typename std::vector<T, A>::size_type n) -> void
  {
    v.reserve(n);
  }

  template<typename T, typename A>
  inline auto reserve_at_least(std::vector<T, A> &v, typename std::vector<T, A>::size_type n) -> void
  {
    if (n <= v.capacity())
      return;
    const auto geom = v.capacity() + (v.capacity() / 2) + 1;
    v.reserve(geom > n ? geom : n);
  }

  template<typename T, typename I, typename A>
  inline auto reserve_exact(containers::CompactVecBase<T, I, A> &v, I n) -> void
  {
    v.reserve_exact(n);
  }

  template<typename T, typename I, typename A>
  inline auto reserve_at_least(containers::CompactVecBase<T, I, A> &v, I n) -> void
  {
    v.reserve_at_least(n);
  }

  template<typename T, typename A> inline auto push(std::vector<T, A> &v, T &&x) -> void
  {
    v.push_back(std::forward<T>(x));
  }

  template<typename T, typename A> inline auto push(std::vector<T, A> &v, const T &x) -> void
  {
    v.push_back(x);
  }

  template<typename T, typename A> inline auto pop(std::vector<T, A> &v) -> void
  {
    if (!v.empty())
      v.pop_back();
  }

  template<typename T, typename I, typename A> inline auto push(containers::CompactVecBase<T, I, A> &v, T &&x) -> void
  {
    v.push(std::forward<T>(x));
  }

  template<typename T, typename I, typename A>
  inline auto push(containers::CompactVecBase<T, I, A> &v, const T &x) -> void
  {
    v.push(x);
  }

  template<typename T, typename I, typename A> inline auto pop(containers::CompactVecBase<T, I, A> &v) -> void
  {
    v.pop();
  }

  template<typename T, typename A> [[nodiscard]] inline auto as_span(std::vector<T, A> &v) noexcept -> std::span<T>
  {
    return std::span<T>(v);
  }

  template<typename T, typename A>
  [[nodiscard]] inline auto as_span(const std::vector<T, A> &v) noexcept -> std::span<const T>
  {
    return std::span<const T>(v);
  }
} // namespace au

export namespace au::containers
{
  inline constexpr u32 INDEX_INVALID = UINT32_MAX;

  [[nodiscard]] u64 hash_bytes(const void *data, usize len, u64 seed = 0) noexcept
  {
    return ::wyhash(data, len, seed, ::_wyp);
  }

  namespace detail
  {
    [[nodiscard]] inline constexpr u64 int_mix(u64 x) noexcept
    {
      auto t = x * 11400714819323198485ULL;
      t ^= (t >> 32);
      return t;
    }

    [[nodiscard]] inline u64 wymix(u64 a, u64 b) noexcept
    {
#if defined(_MSC_VER) && defined(_M_X64)
      u64 hi = 0;
      const u64 lo = _umul128(a, b, &hi);
      return lo ^ hi;
#elif defined(__SIZEOF_INT128__)
      __uint128_t r = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
      return static_cast<u64>(r) ^ static_cast<u64>(r >> 64);
#else
      const u64 ha = a >> 32, la = a & 0xFFFFFFFFULL;
      const u64 hb = b >> 32, lb = b & 0xFFFFFFFFULL;
      const u64 rh = ha * hb;
      const u64 rm0 = ha * lb;
      const u64 rm1 = hb * la;
      const u64 rl = la * lb;
      const u64 t = rl + (rm0 << 32);
      u64 c = static_cast<u64>(t < rl);
      const u64 lo = t + (rm1 << 32);
      c += static_cast<u64>(lo < t);
      const u64 hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
      return lo ^ hi;
#endif
    }

    [[nodiscard]] auto random_seed_64() noexcept -> u64;
  } // namespace detail

  template<typename T> struct Hash;

  template<typename T>
    requires std::has_unique_object_representations_v<T> && (!std::is_pointer_v<T>)
  struct Hash<T>
  {
    [[nodiscard]] u64 operator()(const T &val) const noexcept
    {
      return hash_bytes(&val, sizeof(T));
    }

    [[nodiscard]] u64 operator()(const T &val, u64 seed) const noexcept
    {
      return hash_bytes(&val, sizeof(T), seed);
    }
  };

#define AU_DEFINE_INT_HASH(T)                                                                                          \
  template<> struct Hash<T>                                                                                            \
  {                                                                                                                    \
    [[nodiscard]] u64 operator()(T x) const noexcept                                                                   \
    {                                                                                                                  \
      return detail::int_mix(static_cast<u64>(x));                                                                     \
    }                                                                                                                  \
    [[nodiscard]] u64 operator()(T x, u64 seed) const noexcept                                                         \
    {                                                                                                                  \
      return detail::int_mix(static_cast<u64>(x) ^ seed);                                                              \
    }                                                                                                                  \
  }

  AU_DEFINE_INT_HASH(bool);
  AU_DEFINE_INT_HASH(char);
  AU_DEFINE_INT_HASH(i8);
  AU_DEFINE_INT_HASH(u8);
  AU_DEFINE_INT_HASH(i16);
  AU_DEFINE_INT_HASH(u16);
  AU_DEFINE_INT_HASH(i32);
  AU_DEFINE_INT_HASH(u32);
  AU_DEFINE_INT_HASH(i64);
  AU_DEFINE_INT_HASH(u64);

#undef AU_DEFINE_INT_HASH

  template<typename T> struct Hash<T *>
  {
    [[nodiscard]] u64 operator()(const T *p) const noexcept
    {
      return detail::int_mix(reinterpret_cast<uintptr_t>(p));
    }

    [[nodiscard]] u64 operator()(const T *p, u64 seed) const noexcept
    {
      return detail::int_mix(reinterpret_cast<uintptr_t>(p) ^ seed);
    }
  };

  template<class A> struct Hash<BasicString<A>>
  {
    using is_transparent = void;

    [[nodiscard]] u64 operator()(StringView sv) const noexcept
    {
      return hash_string_view(sv);
    }

    [[nodiscard]] u64 operator()(const char *s) const noexcept
    {
      return hash_string_view(StringView(s));
    }

    [[nodiscard]] u64 operator()(StringView sv, u64 seed) const noexcept
    {
      return hash_bytes(sv.data(), sv.size(), seed);
    }

    [[nodiscard]] u64 operator()(const char *s, u64 seed) const noexcept
    {
      const StringView sv(s);
      return hash_bytes(sv.data(), sv.size(), seed);
    }
  };

  template<> struct Hash<StringView>
  {
    using is_transparent = void;

    [[nodiscard]] u64 operator()(StringView sv) const noexcept
    {
      return hash_string_view(sv);
    }

    [[nodiscard]] u64 operator()(const char *s) const noexcept
    {
      return hash_string_view(StringView(s));
    }

    [[nodiscard]] u64 operator()(StringView sv, u64 seed) const noexcept
    {
      return hash_bytes(sv.data(), sv.size(), seed);
    }

    [[nodiscard]] u64 operator()(const char *s, u64 seed) const noexcept
    {
      const StringView sv(s);
      return hash_bytes(sv.data(), sv.size(), seed);
    }
  };

  template<> struct Hash<const char *>
  {
    [[nodiscard]] u64 operator()(const char *s) const noexcept
    {
      return hash_string_view(StringView(s));
    }

    [[nodiscard]] u64 operator()(const char *s, u64 seed) const noexcept
    {
      const StringView sv(s);
      return hash_bytes(sv.data(), sv.size(), seed);
    }
  };

  template<typename T> struct EqualTo
  {
    constexpr bool operator()(const T &lhs, const T &rhs) const
    {
      return lhs == rhs;
    }
  };

  template<class A> struct EqualTo<BasicString<A>>
  {
    using is_transparent = void;

    constexpr bool operator()(StringView lhs, StringView rhs) const noexcept
    {
      return lhs == rhs;
    }
  };

  template<> struct EqualTo<StringView>
  {
    using is_transparent = void;

    constexpr bool operator()(StringView lhs, StringView rhs) const noexcept
    {
      return lhs == rhs;
    }
  };

  [[nodiscard]] inline u64 hash_combine(u64 seed, u64 value) noexcept
  {
    return seed ^ (detail::int_mix(value) + 0x9E3779B97F4A7C15ULL + (seed << 6) + (seed >> 2));
  }

  template<typename It> [[nodiscard]] inline u64 hash_combine_range(u64 seed, It first, It last) noexcept
  {
    using value_type = std::remove_cvref_t<decltype(*first)>;
    Hash<value_type> hasher{};
    for (; first != last; ++first)
      seed = hash_combine(seed, hasher(*first));
    return seed;
  }

  template<typename K, typename V> struct Hash<Pair<K, V>>
  {
    [[nodiscard]] u64 operator()(const Pair<K, V> &p) const noexcept
    {
      const u64 h1 = Hash<K>{}(p.first);
      const u64 h2 = Hash<V>{}(p.second);
      return hash_combine(h1, h2);
    }

    [[nodiscard]] u64 operator()(const Pair<K, V> &p, u64 seed) const noexcept
    {
      const u64 h1 = Hash<K>{}(p.first, seed);
      const u64 h2 = Hash<V>{}(p.second, seed ^ 0xD6E8FEB86659FD93ULL);
      return hash_combine(h1, h2);
    }
  };

  template<typename... Ts> struct Hash<std::tuple<Ts...>>
  {
    [[nodiscard]] u64 operator()(const std::tuple<Ts...> &t) const noexcept
    {
      return hash_impl(t, std::index_sequence_for<Ts...>{});
    }

    [[nodiscard]] u64 operator()(const std::tuple<Ts...> &t, u64 seed) const noexcept
    {
      return hash_impl_seeded(t, seed, std::index_sequence_for<Ts...>{});
    }

private:
    template<usize... Is>
    [[nodiscard]] static u64 hash_impl(const std::tuple<Ts...> &t, std::index_sequence<Is...>) noexcept
    {
      if constexpr (sizeof...(Ts) == 0)
        return 0;
      else
      {
        u64 seed = 0;
        ((seed = hash_combine(seed, Hash<Ts>{}(std::get<Is>(t)))), ...);
        return seed;
      }
    }

    template<usize... Is>
    [[nodiscard]] static u64 hash_impl_seeded(const std::tuple<Ts...> &t, u64 seed, std::index_sequence<Is...>) noexcept
    {
      if constexpr (sizeof...(Ts) == 0)
        return seed;
      else
      {
        u64 acc = seed;
        ((acc = hash_combine(acc, Hash<Ts>{}(std::get<Is>(t), seed))), ...);
        return acc;
      }
    }
  };

  template<> struct Hash<Span<const u8>>
  {
    [[nodiscard]] u64 operator()(Span<const u8> bytes) const noexcept
    {
      return hash_bytes(bytes.data(), bytes.size());
    }

    [[nodiscard]] u64 operator()(Span<const u8> bytes, u64 seed) const noexcept
    {
      return hash_bytes(bytes.data(), bytes.size(), seed);
    }
  };
} // namespace au::containers

namespace au::containers::swiss
{
  using ctrl_t = u8;

  // 0b1000_0000 = slot has never been occupied.
  // 0b1111_1110 = slot was occupied but its entry was erased.
  // 16 slots loaded per SIMD group.
  inline constexpr ctrl_t kEmpty = static_cast<ctrl_t>(0x80);
  inline constexpr ctrl_t kDeleted = static_cast<ctrl_t>(0xFE);
  inline constexpr usize kGroupWidth = 16;

  [[nodiscard]] inline constexpr bool is_full(ctrl_t c) noexcept
  {
    return (c & static_cast<ctrl_t>(0x80)) == 0;
  }

  // 7-bit fingerprint stored verbatim in the control array. The highest bit
  // is reserved for the empty/deleted markers.
  [[nodiscard]] inline constexpr u8 h2(u64 hash) noexcept
  {
    return static_cast<u8>(hash & 0x7Fu);
  }

  // Upper-57 bits seed the group probe; H2 sits in the lower 7 so attackers
  // controlling H2 alone do not control also the starting group.
  [[nodiscard]] inline constexpr u64 h1(u64 hash) noexcept
  {
    return hash >> 7;
  }

  // Bitmask over a 16-slot group. ith bit set means slot i matched.
  class BitMask
  {
public:
    constexpr explicit BitMask(u32 mask) noexcept : m_mask(mask)
    {
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
      return m_mask != 0;
    }

    [[nodiscard]] constexpr bool any() const noexcept
    {
      return m_mask != 0;
    }

    [[nodiscard]] constexpr u32 lowest() const noexcept
    {
      return static_cast<u32>(std::countr_zero(m_mask));
    }

    constexpr void clear_lowest() noexcept
    {
      m_mask &= (m_mask - 1);
    }

    struct Iterator
    {
      u32 m;

      [[nodiscard]] constexpr u32 operator*() const noexcept
      {
        return static_cast<u32>(std::countr_zero(m));
      }

      constexpr auto operator++() noexcept -> Iterator &
      {
        m &= (m - 1);
        return *this;
      }

      [[nodiscard]] constexpr bool operator!=(const Iterator &other) const noexcept
      {
        return m != other.m;
      }
    };

    [[nodiscard]] constexpr auto begin() const noexcept -> Iterator
    {
      return Iterator{m_mask};
    }

    [[nodiscard]] constexpr auto end() const noexcept -> Iterator
    {
      return Iterator{0};
    }

private:
    u32 m_mask;
  };

  // 16-byte SIMD probe group
  class Group
  {
public:
#if defined(AUXID_HAS_SSE2) && !defined(AUXID_DISABLE_HASHMAP_SIMD)
    explicit Group(const ctrl_t *p) noexcept : m_vec(_mm_loadu_si128(reinterpret_cast<const __m128i *>(p)))
    {
    }

    [[nodiscard]] BitMask match(u8 needle) const noexcept
    {
      const __m128i needle_vec = _mm_set1_epi8(static_cast<char>(needle));
      const __m128i eq = _mm_cmpeq_epi8(needle_vec, m_vec);
      return BitMask(static_cast<u32>(_mm_movemask_epi8(eq)) & 0xFFFFu);
    }

    [[nodiscard]] BitMask match_empty() const noexcept
    {
      const __m128i needle_vec = _mm_set1_epi8(static_cast<char>(0x80));
      const __m128i eq = _mm_cmpeq_epi8(needle_vec, m_vec);
      return BitMask(static_cast<u32>(_mm_movemask_epi8(eq)) & 0xFFFFu);
    }

    [[nodiscard]] BitMask match_empty_or_deleted() const noexcept
    {
      return BitMask(static_cast<u32>(_mm_movemask_epi8(m_vec)) & 0xFFFFu);
    }

private:
    __m128i m_vec;

#elif defined(AUXID_HAS_NEON) && !defined(AUXID_DISABLE_HASHMAP_SIMD)
    explicit Group(const ctrl_t *p) noexcept : m_vec(vld1q_u8(p))
    {
    }

    [[nodiscard]] BitMask match(u8 needle) const noexcept
    {
      const uint8x16_t needle_vec = vdupq_n_u8(needle);
      const uint8x16_t eq = vceqq_u8(needle_vec, m_vec);
      return BitMask(neon_movemask(eq));
    }

    [[nodiscard]] BitMask match_empty() const noexcept
    {
      const uint8x16_t needle_vec = vdupq_n_u8(static_cast<u8>(0x80));
      const uint8x16_t eq = vceqq_u8(needle_vec, m_vec);
      return BitMask(neon_movemask(eq));
    }

    [[nodiscard]] BitMask match_empty_or_deleted() const noexcept
    {
      const uint8x16_t high_bit_mask = vdupq_n_u8(static_cast<u8>(0x80));
      const uint8x16_t high_bit = vandq_u8(m_vec, high_bit_mask);
      const uint8x16_t set = vceqq_u8(high_bit, high_bit_mask);
      return BitMask(neon_movemask(set));
    }

private:
    uint8x16_t m_vec;

    [[nodiscard]] static u32 neon_movemask(uint8x16_t v) noexcept
    {
      alignas(16) static constexpr u8 kBitMaskBytes[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                                                           0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
      const uint8x16_t bit_mask = vld1q_u8(kBitMaskBytes);
      const uint8x16_t masked = vandq_u8(v, bit_mask);
      const u32 lo = static_cast<u32>(vaddv_u8(vget_low_u8(masked)));
      const u32 hi = static_cast<u32>(vaddv_u8(vget_high_u8(masked)));
      return lo | (hi << 8);
    }

#elif defined(AUXID_HAS_WASM_SIMD128) && !defined(AUXID_DISABLE_HASHMAP_SIMD)
    explicit Group(const ctrl_t *p) noexcept : m_vec(wasm_v128_load(p))
    {
    }

    [[nodiscard]] BitMask match(u8 needle) const noexcept
    {
      const v128_t needle_vec = wasm_i8x16_splat(static_cast<int8_t>(needle));
      const v128_t eq = wasm_i8x16_eq(needle_vec, m_vec);
      return BitMask(static_cast<u32>(wasm_i8x16_bitmask(eq)) & 0xFFFFu);
    }

    [[nodiscard]] BitMask match_empty() const noexcept
    {
      const v128_t needle_vec = wasm_i8x16_splat(static_cast<int8_t>(0x80));
      const v128_t eq = wasm_i8x16_eq(needle_vec, m_vec);
      return BitMask(static_cast<u32>(wasm_i8x16_bitmask(eq)) & 0xFFFFu);
    }

    [[nodiscard]] BitMask match_empty_or_deleted() const noexcept
    {
      return BitMask(static_cast<u32>(wasm_i8x16_bitmask(m_vec)) & 0xFFFFu);
    }

private:
    v128_t m_vec;

#else
    explicit Group(const ctrl_t *p) noexcept
    {
      std::memcpy(&m_lo, p, sizeof(u64));
      std::memcpy(&m_hi, p + 8, sizeof(u64));
    }

    [[nodiscard]] BitMask match(u8 needle) const noexcept
    {
      const u64 broadcast = 0x0101010101010101ULL * static_cast<u64>(needle);
      const u64 lx = m_lo ^ broadcast;
      const u64 hx = m_hi ^ broadcast;
      const u64 lm = (lx - 0x0101010101010101ULL) & ~lx & 0x8080808080808080ULL;
      const u64 hm = (hx - 0x0101010101010101ULL) & ~hx & 0x8080808080808080ULL;
      return BitMask(pack_high_bits(lm) | (pack_high_bits(hm) << 8));
    }

    [[nodiscard]] BitMask match_empty() const noexcept
    {
      constexpr u64 broadcast = 0x8080808080808080ULL;
      const u64 lx = m_lo ^ broadcast;
      const u64 hx = m_hi ^ broadcast;
      const u64 lm = (lx - 0x0101010101010101ULL) & ~lx & 0x8080808080808080ULL;
      const u64 hm = (hx - 0x0101010101010101ULL) & ~hx & 0x8080808080808080ULL;
      return BitMask(pack_high_bits(lm) | (pack_high_bits(hm) << 8));
    }

    [[nodiscard]] BitMask match_empty_or_deleted() const noexcept
    {
      const u64 lm = m_lo & 0x8080808080808080ULL;
      const u64 hm = m_hi & 0x8080808080808080ULL;
      return BitMask(pack_high_bits(lm) | (pack_high_bits(hm) << 8));
    }

private:
    u64 m_lo = 0;
    u64 m_hi = 0;

    [[nodiscard]] static u32 pack_high_bits(u64 v) noexcept
    {
      u32 m = 0;
      m |= static_cast<u32>((v >> 7) & 1u) << 0;
      m |= static_cast<u32>((v >> 15) & 1u) << 1;
      m |= static_cast<u32>((v >> 23) & 1u) << 2;
      m |= static_cast<u32>((v >> 31) & 1u) << 3;
      m |= static_cast<u32>((v >> 39) & 1u) << 4;
      m |= static_cast<u32>((v >> 47) & 1u) << 5;
      m |= static_cast<u32>((v >> 55) & 1u) << 6;
      m |= static_cast<u32>((v >> 63) & 1u) << 7;
      return m;
    }
#endif
  };

  struct ProbeSeq
  {
    usize group_index;
    usize step;
    usize n_groups_mask;

    constexpr ProbeSeq(u64 hash, usize ngm) noexcept
        : group_index(static_cast<usize>(h1(hash)) & ngm), step(0), n_groups_mask(ngm)
    {
    }

    [[nodiscard]] constexpr usize offset() const noexcept
    {
      return group_index * kGroupWidth;
    }

    constexpr void next() noexcept
    {
      ++step;
      group_index = (group_index + step) & n_groups_mask;
    }
  };
} // namespace au::containers::swiss

export namespace au::containers
{
  struct PairFirstKeyOf
  {
    template<class P> [[nodiscard]] constexpr auto operator()(const P &p) const noexcept -> const auto &
    {
      return p.first;
    }
  };

  struct IdentityKeyOf
  {
    template<class T> [[nodiscard]] constexpr auto operator()(const T &v) const noexcept -> const T &
    {
      return v;
    }
  };

  template<class Hasher, class KeyEq, class Key, class K2>
  concept TransparentLookup = requires { typename Hasher::is_transparent; } && requires {
    typename KeyEq::is_transparent;
  } && requires(const Hasher &h, const KeyEq &e, const K2 &k2, const Key &k) {
    { h(k2) } -> std::convertible_to<u64>;
    { e(k2, k) } -> std::convertible_to<bool>;
  };

  template<class H, class K>
  concept SeedableHasher = requires(const H h, const K &k, u64 s) {
    { h(k, s) } -> std::convertible_to<u64>;
  };

  template<class Entry, class Key, class KeyOf, class Hasher = Hash<Key>, class KeyEq = EqualTo<Key>,
           class AllocatorT = memory::HeapAllocator>
    requires memory::AllocatorType<AllocatorT>
  class HashTable
  {
public:
    using entry_type = Entry;
    using key_type = Key;
    using size_type = usize;
    using difference_type = isize;
    using reference = Entry &;
    using const_reference = const Entry &;
    using pointer = Entry *;
    using const_pointer = const Entry *;
    using iterator = Entry *;
    using const_iterator = const Entry *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    using ctrl_t = swiss::ctrl_t;
    static constexpr usize kGroupWidth = swiss::kGroupWidth;
    static constexpr usize kCtrlAlign = 16;

    VecT<Entry, usize, AllocatorT> m_entries{};
    ctrl_t *m_ctrl = nullptr;
    u32 *m_index = nullptr;
    size_type m_capacity = 0;    // n_slots, power-of-two multiple of 16 (or zero)
    size_type m_groups_mask = 0; // n_groups - 1
    size_type m_tombstones = 0;
    u64 m_seed = 0;

    AUXID_NO_UNIQUE_ADDRESS Hasher m_hasher{};
    AUXID_NO_UNIQUE_ADDRESS KeyEq m_eq{};
    AUXID_NO_UNIQUE_ADDRESS KeyOf m_key_of{};

public:
    HashTable() noexcept : m_seed(detail::random_seed_64())
    {
    }

    explicit HashTable(size_type cap) : m_seed(detail::random_seed_64())
    {
      if (cap > 0)
        reserve(cap);
    }

    HashTable(size_type cap, u64 seed) : m_seed(seed)
    {
      if (cap > 0)
        reserve(cap);
    }

    ~HashTable()
    {
      free_buffer();
    }

    HashTable(const HashTable &other)
        : m_entries(other.m_entries), m_seed(other.m_seed), m_hasher(other.m_hasher), m_eq(other.m_eq),
          m_key_of(other.m_key_of)
    {
      if (other.m_capacity > 0)
      {
        allocate_buffer(other.m_capacity);
        std::memcpy(m_ctrl, other.m_ctrl, other.m_capacity);
        std::memcpy(m_index, other.m_index, other.m_capacity * sizeof(u32));
        m_tombstones = other.m_tombstones;
      }
    }

    HashTable &operator=(const HashTable &other)
    {
      if (this != &other)
      {
        HashTable tmp(other);
        *this = std::move(tmp);
      }
      return *this;
    }

    HashTable(HashTable &&other) noexcept
        : m_entries(std::move(other.m_entries)), m_ctrl(other.m_ctrl), m_index(other.m_index),
          m_capacity(other.m_capacity), m_groups_mask(other.m_groups_mask), m_tombstones(other.m_tombstones),
          m_seed(other.m_seed), m_hasher(std::move(other.m_hasher)), m_eq(std::move(other.m_eq)),
          m_key_of(std::move(other.m_key_of))
    {
      other.m_ctrl = nullptr;
      other.m_index = nullptr;
      other.m_capacity = 0;
      other.m_groups_mask = 0;
      other.m_tombstones = 0;
    }

    HashTable &operator=(HashTable &&other) noexcept
    {
      if (this != &other)
      {
        free_buffer();
        m_entries = std::move(other.m_entries);
        m_ctrl = other.m_ctrl;
        m_index = other.m_index;
        m_capacity = other.m_capacity;
        m_groups_mask = other.m_groups_mask;
        m_tombstones = other.m_tombstones;
        m_seed = other.m_seed;
        m_hasher = std::move(other.m_hasher);
        m_eq = std::move(other.m_eq);
        m_key_of = std::move(other.m_key_of);
        other.m_ctrl = nullptr;
        other.m_index = nullptr;
        other.m_capacity = 0;
        other.m_groups_mask = 0;
        other.m_tombstones = 0;
      }
      return *this;
    }

    void reserve_at_least(size_type new_cap)
    {
      if (new_cap == 0)
        return;

      const size_type needed_slots = (new_cap * 8 + 6) / 7;
      size_type new_slots = kGroupWidth;
      while (new_slots < needed_slots)
        new_slots <<= 1;
      if (new_slots <= m_capacity)
        return;
      m_entries.reserve(new_cap);
      rehash_to(new_slots);
    }

    void reserve_exact(size_type new_cap)
    {
      reserve_at_least(new_cap);
    }

    void reserve(size_type new_cap)
    {
      reserve_at_least(new_cap);
    }

    void clear()
    {
      m_entries.clear();
      if (m_ctrl)
        std::memset(m_ctrl, swiss::kEmpty, m_capacity);
      m_tombstones = 0;
    }

    bool try_insert(Entry &&entry)
    {
      if (contains(m_key_of(entry)))
        return false;
      insert_unique(std::move(entry));
      return true;
    }

    bool try_insert(const Entry &entry)
    {
      Entry copy = entry;
      return try_insert(std::move(copy));
    }

    template<class Factory> Entry &find_or_emplace(const Key &key, Factory &&factory)
    {
      ensure_growth_room();
      return find_or_emplace_locked(key, std::forward<Factory>(factory));
    }

    [[nodiscard]] Entry *find_entry(const Key &key)
    {
      return find_entry_impl(key);
    }

    [[nodiscard]] const Entry *find_entry(const Key &key) const
    {
      return find_entry_impl(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, Key, K2>
    [[nodiscard]] Entry *find_entry(const K2 &key)
    {
      return find_entry_impl(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, Key, K2>
    [[nodiscard]] const Entry *find_entry(const K2 &key) const
    {
      return find_entry_impl(key);
    }

    [[nodiscard]] bool contains(const Key &key) const
    {
      return find_entry(key) != nullptr;
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, Key, K2>
    [[nodiscard]] bool contains(const K2 &key) const
    {
      return find_entry(key) != nullptr;
    }

    bool erase(const Key &key)
    {
      return erase_impl(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, Key, K2>
    bool erase(const K2 &key)
    {
      return erase_impl(key);
    }

    [[nodiscard]] iterator begin() noexcept
    {
      return m_entries.begin();
    }

    [[nodiscard]] iterator end() noexcept
    {
      return m_entries.end();
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
      return m_entries.begin();
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
      return m_entries.end();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
      return begin();
    }

    [[nodiscard]] const_iterator cend() const noexcept
    {
      return end();
    }

    [[nodiscard]] reverse_iterator rbegin() noexcept
    {
      return reverse_iterator(end());
    }

    [[nodiscard]] reverse_iterator rend() noexcept
    {
      return reverse_iterator(begin());
    }

    [[nodiscard]] const_reverse_iterator rbegin() const noexcept
    {
      return const_reverse_iterator(end());
    }

    [[nodiscard]] const_reverse_iterator rend() const noexcept
    {
      return const_reverse_iterator(begin());
    }

    [[nodiscard]] const_reverse_iterator crbegin() const noexcept
    {
      return rbegin();
    }

    [[nodiscard]] const_reverse_iterator crend() const noexcept
    {
      return rend();
    }

    [[nodiscard]] size_type size() const noexcept
    {
      return m_entries.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
      return m_entries.empty();
    }

    [[nodiscard]] u64 seed() const noexcept
    {
      return m_seed;
    }

    [[nodiscard]] size_type capacity_slots() const noexcept
    {
      return m_capacity;
    }

    [[nodiscard]] size_type tombstones() const noexcept
    {
      return m_tombstones;
    }

private:
    template<class K> [[nodiscard]] u64 hash_key(const K &key) const noexcept
    {
      if constexpr (SeedableHasher<Hasher, K>)
        return m_hasher(key, m_seed);
      else
        return detail::wymix(m_hasher(key), m_seed ^ 0xBF58476D1CE4E5B9ULL);
    }

    [[nodiscard]] static size_type max_load_for(size_type n_slots) noexcept
    {
      return n_slots - (n_slots / 8);
    }

    [[nodiscard]] bool has_room_for_one() const noexcept
    {
      if (m_capacity == 0)
        return false;
      return (m_entries.size() + m_tombstones) < max_load_for(m_capacity);
    }

    void ensure_growth_room()
    {
      if (has_room_for_one())
        return;
      if (m_capacity == 0)
      {
        rehash_to(kGroupWidth);
        return;
      }
      if (m_entries.size() * 2 <= m_capacity)
        drop_deleted_in_place();
      else
        rehash_to(m_capacity * 2);
    }

    void allocate_buffer(size_type n_slots)
    {
      const size_type ctrl_bytes = n_slots * sizeof(ctrl_t);
      constexpr size_type idx_align = alignof(u32);
      const size_type ctrl_padded = (ctrl_bytes + idx_align - 1) & ~(idx_align - 1);
      const size_type idx_bytes = n_slots * sizeof(u32);
      const size_type total = ctrl_padded + idx_bytes;

      AllocatorT alloc{};
      void *raw = alloc.alloc(total, kCtrlAlign);
      m_ctrl = static_cast<ctrl_t *>(raw);
      m_index = reinterpret_cast<u32 *>(static_cast<u8 *>(raw) + ctrl_padded);
      std::memset(m_ctrl, swiss::kEmpty, ctrl_bytes);
      m_capacity = n_slots;
      m_groups_mask = (n_slots / kGroupWidth) - 1;
      m_tombstones = 0;
    }

    void free_buffer() noexcept
    {
      if (!m_ctrl)
        return;
      const size_type ctrl_bytes = m_capacity * sizeof(ctrl_t);
      constexpr size_type idx_align = alignof(u32);
      const size_type ctrl_padded = (ctrl_bytes + idx_align - 1) & ~(idx_align - 1);
      const size_type idx_bytes = m_capacity * sizeof(u32);
      const size_type total = ctrl_padded + idx_bytes;
      AllocatorT alloc{};
      alloc.free(m_ctrl, total, kCtrlAlign);
      m_ctrl = nullptr;
      m_index = nullptr;
      m_capacity = 0;
      m_groups_mask = 0;
      m_tombstones = 0;
    }

    void rehash_to(size_type new_n_slots)
    {
      free_buffer();
      allocate_buffer(new_n_slots);
      for (u32 i = 0; i < static_cast<u32>(m_entries.size()); ++i)
        place_unique(i);
    }

    void drop_deleted_in_place()
    {
      if (m_capacity == 0)
        return;
      std::memset(m_ctrl, swiss::kEmpty, m_capacity);
      m_tombstones = 0;
      for (u32 i = 0; i < static_cast<u32>(m_entries.size()); ++i)
        place_unique(i);
    }

    void place_unique(u32 entry_idx) noexcept
    {
      const auto &key = m_key_of(m_entries[entry_idx]);
      const u64 h = hash_key(key);
      const u8 h2v = swiss::h2(h);
      swiss::ProbeSeq seq(h, m_groups_mask);
      for (;;)
      {
        const swiss::Group g(m_ctrl + seq.offset());
        if (auto ed = g.match_empty_or_deleted(); ed.any())
        {
          const size_type slot = seq.offset() + ed.lowest();
          m_ctrl[slot] = static_cast<ctrl_t>(h2v);
          m_index[slot] = entry_idx;
          return;
        }
        seq.next();
      }
    }

    void insert_unique(Entry &&entry)
    {
      ensure_growth_room();
      const u32 entry_idx = static_cast<u32>(m_entries.size());
      m_entries.push(std::move(entry));
      place_unique(entry_idx);
    }

    template<class Factory> Entry &find_or_emplace_locked(const Key &key, Factory &&factory)
    {
      const u64 h = hash_key(key);
      const u8 h2v = swiss::h2(h);
      swiss::ProbeSeq seq(h, m_groups_mask);
      constexpr size_type kNoSlot = static_cast<size_type>(-1);
      size_type tombstone_slot = kNoSlot;

      for (size_type i = 0; i <= m_groups_mask; ++i)
      {
        const swiss::Group g(m_ctrl + seq.offset());

        for (auto m = g.match(h2v); m.any(); m.clear_lowest())
        {
          const u32 bit = m.lowest();
          const u32 entry_idx = m_index[seq.offset() + bit];
          if AU_LIKELY (m_eq(m_key_of(m_entries[entry_idx]), key))
            return m_entries[entry_idx];
        }

        if (tombstone_slot == kNoSlot)
        {
          if (auto ed = g.match_empty_or_deleted(); ed.any())
          {
            const size_type slot = seq.offset() + ed.lowest();
            if (m_ctrl[slot] == swiss::kDeleted)
              tombstone_slot = slot;
          }
        }

        if (auto e = g.match_empty(); e.any())
        {
          const size_type empty_slot = seq.offset() + e.lowest();
          const bool reuse_tomb = (tombstone_slot != kNoSlot);
          const size_type place = reuse_tomb ? tombstone_slot : empty_slot;

          const u32 entry_idx = static_cast<u32>(m_entries.size());
          m_entries.push(static_cast<Factory &&>(factory)());
          m_ctrl[place] = static_cast<ctrl_t>(h2v);
          m_index[place] = entry_idx;
          if (reuse_tomb)
            --m_tombstones;
          return m_entries[entry_idx];
        }

        seq.next();
      }
      panic("HashTable::find_or_emplace: probe sequence exhausted");
    }

    template<class K> [[nodiscard]] Entry *find_entry_impl(const K &key) const
    {
      if (m_capacity == 0)
        return nullptr;
      const u64 h = hash_key(key);
      const u8 h2v = swiss::h2(h);
      swiss::ProbeSeq seq(h, m_groups_mask);

      for (size_type i = 0; i <= m_groups_mask; ++i)
      {
        const swiss::Group g(m_ctrl + seq.offset());
        for (auto m = g.match(h2v); m.any(); m.clear_lowest())
        {
          const u32 bit = m.lowest();
          const u32 entry_idx = m_index[seq.offset() + bit];
          if AU_LIKELY (m_eq(m_key_of(m_entries[entry_idx]), key))
            return const_cast<Entry *>(&m_entries[entry_idx]);
        }
        if (g.match_empty().any())
          return nullptr;
        seq.next();
      }
      return nullptr;
    }

    template<class K> bool erase_impl(const K &key)
    {
      if (m_capacity == 0)
        return false;
      const u64 h = hash_key(key);
      const u8 h2v = swiss::h2(h);
      swiss::ProbeSeq seq(h, m_groups_mask);

      for (size_type i = 0; i <= m_groups_mask; ++i)
      {
        const swiss::Group g(m_ctrl + seq.offset());
        for (auto m = g.match(h2v); m.any(); m.clear_lowest())
        {
          const u32 bit = m.lowest();
          const size_type slot = seq.offset() + bit;
          const u32 entry_idx = m_index[slot];
          if (m_eq(m_key_of(m_entries[entry_idx]), key))
          {
            m_ctrl[slot] = swiss::kDeleted;
            ++m_tombstones;

            const u32 last_idx = static_cast<u32>(m_entries.size() - 1);
            if (entry_idx != last_idx)
            {
              m_entries[entry_idx] = std::move(m_entries[last_idx]);
              relocate_index(m_key_of(m_entries[entry_idx]), last_idx, entry_idx);
            }
            m_entries.pop();
            return true;
          }
        }
        if (g.match_empty().any())
          return false;
        seq.next();
      }
      return false;
    }

    template<class K> void relocate_index(const K &key, u32 old_idx, u32 new_idx) noexcept
    {
      const u64 h = hash_key(key);
      const u8 h2v = swiss::h2(h);
      swiss::ProbeSeq seq(h, m_groups_mask);
      for (;;)
      {
        const swiss::Group g(m_ctrl + seq.offset());
        for (auto m = g.match(h2v); m.any(); m.clear_lowest())
        {
          const u32 bit = m.lowest();
          const size_type slot = seq.offset() + bit;
          if (m_index[slot] == old_idx)
          {
            m_index[slot] = new_idx;
            return;
          }
        }
        seq.next();
      }
    }
  };
} // namespace au::containers

export namespace au::containers
{
  template<typename K, typename V, typename Hasher = Hash<K>, typename KeyEq = EqualTo<K>,
           typename AllocatorT = memory::HeapAllocator>
    requires memory::AllocatorType<AllocatorT>
  class HashMap
  {
public:
    using value_type = Pair<K, V>;
    using key_type = K;
    using mapped_type = V;
    using size_type = usize;
    using difference_type = isize;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = value_type *;
    using const_iterator = const value_type *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    using Table = HashTable<value_type, K, PairFirstKeyOf, Hasher, KeyEq, AllocatorT>;
    Table m_table;

public:
    HashMap() = default;

    explicit HashMap(size_type cap) : m_table(cap)
    {
    }

    HashMap(size_type cap, u64 seed) : m_table(cap, seed)
    {
    }

    [[nodiscard]] u64 seed() const noexcept
    {
      return m_table.seed();
    }

    void reserve_at_least(size_type new_cap)
    {
      m_table.reserve_at_least(new_cap);
    }

    void reserve_exact(size_type new_cap)
    {
      m_table.reserve_exact(new_cap);
    }

    void reserve(size_type new_cap)
    {
      m_table.reserve(new_cap);
    }

    void clear()
    {
      m_table.clear();
    }

    V &operator[](const K &key)
    {
      return m_table.find_or_emplace(key, [&key]() { return value_type{key, V{}}; }).second;
    }

    bool insert(const K &key, const V &val)
    {
      return m_table.try_insert(value_type{key, val});
    }

    bool insert(const K &key, V &&val)
    {
      return m_table.try_insert(value_type{key, std::move(val)});
    }

    [[nodiscard]] V *find(const K &key)
    {
      auto *entry = m_table.find_entry(key);
      return entry ? &entry->second : nullptr;
    }

    [[nodiscard]] const V *find(const K &key) const
    {
      auto *entry = m_table.find_entry(key);
      return entry ? &entry->second : nullptr;
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, K, K2>
    [[nodiscard]] V *find(const K2 &key)
    {
      auto *entry = m_table.find_entry(key);
      return entry ? &entry->second : nullptr;
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, K, K2>
    [[nodiscard]] const V *find(const K2 &key) const
    {
      auto *entry = m_table.find_entry(key);
      return entry ? &entry->second : nullptr;
    }

    [[nodiscard]] bool contains(const K &key) const
    {
      return m_table.contains(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, K, K2>
    [[nodiscard]] bool contains(const K2 &key) const
    {
      return m_table.contains(key);
    }

    bool erase(const K &key)
    {
      return m_table.erase(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, K, K2>
    bool erase(const K2 &key)
    {
      return m_table.erase(key);
    }

    [[nodiscard]] iterator begin() noexcept
    {
      return m_table.begin();
    }

    [[nodiscard]] iterator end() noexcept
    {
      return m_table.end();
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
      return m_table.begin();
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
      return m_table.end();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
      return m_table.cbegin();
    }

    [[nodiscard]] const_iterator cend() const noexcept
    {
      return m_table.cend();
    }

    [[nodiscard]] reverse_iterator rbegin() noexcept
    {
      return m_table.rbegin();
    }

    [[nodiscard]] reverse_iterator rend() noexcept
    {
      return m_table.rend();
    }

    [[nodiscard]] const_reverse_iterator rbegin() const noexcept
    {
      return m_table.rbegin();
    }

    [[nodiscard]] const_reverse_iterator rend() const noexcept
    {
      return m_table.rend();
    }

    [[nodiscard]] const_reverse_iterator crbegin() const noexcept
    {
      return m_table.crbegin();
    }

    [[nodiscard]] const_reverse_iterator crend() const noexcept
    {
      return m_table.crend();
    }

    [[nodiscard]] size_type size() const noexcept
    {
      return m_table.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
      return m_table.empty();
    }
  };

  template<typename K, typename Hasher = Hash<K>, typename KeyEq = EqualTo<K>,
           typename AllocatorT = memory::HeapAllocator>
    requires memory::AllocatorType<AllocatorT>
  class HashSet
  {
public:
    using value_type = K;
    using key_type = K;
    using size_type = usize;
    using difference_type = isize;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = value_type *;
    using const_iterator = const value_type *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    using Table = HashTable<K, K, IdentityKeyOf, Hasher, KeyEq, AllocatorT>;
    Table m_table;

public:
    HashSet() = default;

    explicit HashSet(size_type cap) : m_table(cap)
    {
    }

    HashSet(size_type cap, u64 seed) : m_table(cap, seed)
    {
    }

    [[nodiscard]] u64 seed() const noexcept
    {
      return m_table.seed();
    }

    void reserve_at_least(size_type new_cap)
    {
      m_table.reserve_at_least(new_cap);
    }

    void reserve_exact(size_type new_cap)
    {
      m_table.reserve_exact(new_cap);
    }

    void reserve(size_type new_cap)
    {
      m_table.reserve(new_cap);
    }

    void clear()
    {
      m_table.clear();
    }

    bool insert(const K &key)
    {
      return m_table.try_insert(K{key});
    }

    bool insert(K &&key)
    {
      return m_table.try_insert(std::move(key));
    }

    [[nodiscard]] bool contains(const K &key) const
    {
      return m_table.contains(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, K, K2>
    [[nodiscard]] bool contains(const K2 &key) const
    {
      return m_table.contains(key);
    }

    bool erase(const K &key)
    {
      return m_table.erase(key);
    }

    template<class K2>
      requires TransparentLookup<Hasher, KeyEq, K, K2>
    bool erase(const K2 &key)
    {
      return m_table.erase(key);
    }

    [[nodiscard]] iterator begin() noexcept
    {
      return m_table.begin();
    }

    [[nodiscard]] iterator end() noexcept
    {
      return m_table.end();
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
      return m_table.begin();
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
      return m_table.end();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
      return m_table.cbegin();
    }

    [[nodiscard]] const_iterator cend() const noexcept
    {
      return m_table.cend();
    }

    [[nodiscard]] reverse_iterator rbegin() noexcept
    {
      return m_table.rbegin();
    }

    [[nodiscard]] reverse_iterator rend() noexcept
    {
      return m_table.rend();
    }

    [[nodiscard]] const_reverse_iterator rbegin() const noexcept
    {
      return m_table.rbegin();
    }

    [[nodiscard]] const_reverse_iterator rend() const noexcept
    {
      return m_table.rend();
    }

    [[nodiscard]] const_reverse_iterator crbegin() const noexcept
    {
      return m_table.crbegin();
    }

    [[nodiscard]] const_reverse_iterator crend() const noexcept
    {
      return m_table.crend();
    }

    [[nodiscard]] size_type size() const noexcept
    {
      return m_table.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
      return m_table.empty();
    }
  };
} // namespace au::containers

export namespace au
{
  template<typename K, typename V> using HashMap = containers::HashMap<K, V>;
  template<typename T> using HashSet = containers::HashSet<T>;
} // namespace au

export namespace au::containers
{
  template<typename T, memory::AllocatorType AllocatorT = memory::HeapAllocator>
    requires memory::AllocatorType<AllocatorT>
  class SlotMap
  {
public:
    struct Key
    {
      u32 idx;
      u32 gen;

      [[nodiscard]] constexpr bool operator==(const Key &other) const noexcept
      {
        return idx == other.idx && gen == other.gen;
      }
    };

    using value_type = T;
    using size_type = u32;
    using difference_type = std::make_signed_t<u32>;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    struct Slot
    {
      u32 generation;
      u32 dense_idx;
    };

    static constexpr u32 FREE_SENTINEL = static_cast<u32>(-1);

    CompactVecBase<Slot, u32, AllocatorT> m_slots;
    CompactVecBase<T, u32, AllocatorT> m_dense;
    CompactVecBase<u32, u32, AllocatorT> m_dense_to_slot;
    u32 m_free_head = FREE_SENTINEL;

public:
    SlotMap() = default;

    [[nodiscard]] auto insert(T value) -> Key
    {
      u32 slot_idx;
      if (m_free_head != FREE_SENTINEL)
      {
        slot_idx = m_free_head;
        m_free_head = m_slots[slot_idx].dense_idx;
      }
      else
      {
        slot_idx = static_cast<u32>(m_slots.size());
        m_slots.push_back(Slot{0u, 0u});
      }

      const u32 dense_idx = static_cast<u32>(m_dense.size());
      m_dense.push_back(std::move(value));
      m_dense_to_slot.push_back(slot_idx);

      m_slots[slot_idx].dense_idx = dense_idx;
      return Key{slot_idx, m_slots[slot_idx].generation};
    }

    auto erase(Key k) -> bool
    {
      if (k.idx >= m_slots.size())
        return false;
      Slot &slot = m_slots[k.idx];
      if (slot.generation != k.gen)
        return false;

      const u32 dense_idx = slot.dense_idx;
      const u32 last_dense_idx = static_cast<u32>(m_dense.size() - 1);

      if (dense_idx != last_dense_idx)
      {
        m_dense[dense_idx] = std::move(m_dense[last_dense_idx]);
        const u32 moved_slot_idx = m_dense_to_slot[last_dense_idx];
        m_dense_to_slot[dense_idx] = moved_slot_idx;
        m_slots[moved_slot_idx].dense_idx = dense_idx;
      }
      m_dense.pop_back();
      m_dense_to_slot.pop_back();

      slot.generation += 1;
      slot.dense_idx = m_free_head;
      m_free_head = k.idx;
      return true;
    }

    [[nodiscard]] auto get(Key k) -> T *
    {
      if (k.idx >= m_slots.size())
        return nullptr;
      const Slot &slot = m_slots[k.idx];
      if (slot.generation != k.gen)
        return nullptr;
      return &m_dense[slot.dense_idx];
    }

    [[nodiscard]] auto get(Key k) const -> const T *
    {
      if (k.idx >= m_slots.size())
        return nullptr;
      const Slot &slot = m_slots[k.idx];
      if (slot.generation != k.gen)
        return nullptr;
      return &m_dense[slot.dense_idx];
    }

    [[nodiscard]] auto contains(Key k) const -> bool
    {
      return get(k) != nullptr;
    }

    [[nodiscard]] auto size() const noexcept -> size_type
    {
      return static_cast<size_type>(m_dense.size());
    }

    [[nodiscard]] auto empty() const noexcept -> bool
    {
      return m_dense.empty();
    }

    auto clear() -> void
    {
      m_dense.clear();
      m_dense_to_slot.clear();
      m_slots.clear();
      m_free_head = FREE_SENTINEL;
    }

    [[nodiscard]] auto data() noexcept -> T *
    {
      return m_dense.data();
    }

    [[nodiscard]] auto data() const noexcept -> const T *
    {
      return m_dense.data();
    }

    [[nodiscard]] auto begin() noexcept -> iterator
    {
      return m_dense.begin();
    }

    [[nodiscard]] auto end() noexcept -> iterator
    {
      return m_dense.end();
    }

    [[nodiscard]] auto begin() const noexcept -> const_iterator
    {
      return m_dense.begin();
    }

    [[nodiscard]] auto end() const noexcept -> const_iterator
    {
      return m_dense.end();
    }

    [[nodiscard]] auto cbegin() const noexcept -> const_iterator
    {
      return m_dense.cbegin();
    }

    [[nodiscard]] auto cend() const noexcept -> const_iterator
    {
      return m_dense.cend();
    }

    [[nodiscard]] auto rbegin() noexcept -> reverse_iterator
    {
      return reverse_iterator(end());
    }

    [[nodiscard]] auto rend() noexcept -> reverse_iterator
    {
      return reverse_iterator(begin());
    }

    [[nodiscard]] auto rbegin() const noexcept -> const_reverse_iterator
    {
      return const_reverse_iterator(end());
    }

    [[nodiscard]] auto rend() const noexcept -> const_reverse_iterator
    {
      return const_reverse_iterator(begin());
    }

    [[nodiscard]] auto crbegin() const noexcept -> const_reverse_iterator
    {
      return rbegin();
    }

    [[nodiscard]] auto crend() const noexcept -> const_reverse_iterator
    {
      return rend();
    }
  };
} // namespace au::containers

export namespace au
{
  template<typename T, memory::AllocatorType A = memory::HeapAllocator> using SlotMap = containers::SlotMap<T, A>;
} // namespace au

export namespace au::containers
{
  template<typename T, usize Capacity>
    requires((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0))
  class SpscQueue
  {
public:
    SpscQueue() = default;

    ~SpscQueue()
    {
      T dummy;
      while (pop(dummy))
      {
      }
    }

    SpscQueue(const SpscQueue &) = delete;
    SpscQueue &operator=(const SpscQueue &) = delete;

    [[nodiscard]] bool push(T value)
    {
      const usize write_idx = m_write_pos.load(std::memory_order_relaxed);

      const usize read_idx = m_read_pos.load(std::memory_order_acquire);

      if (write_idx - read_idx == Capacity)
        return false;

      T *slot = reinterpret_cast<T *>(&m_slots[(write_idx & K_MASK) * sizeof(T)]);

      new (slot) T(std::move(value));

      m_write_pos.store(write_idx + 1, std::memory_order_release);
      return true;
    }

    [[nodiscard]] bool pop(T &out_value)
    {
      const usize read_idx = m_read_pos.load(std::memory_order_relaxed);

      const usize write_idx = m_write_pos.load(std::memory_order_acquire);

      if (read_idx == write_idx)
        return false;

      T *slot = reinterpret_cast<T *>(&m_slots[(read_idx & K_MASK) * sizeof(T)]);

      out_value = std::move(*slot);

      slot->~T();

      m_read_pos.store(read_idx + 1, std::memory_order_release);
      return true;
    }

private:
    static constexpr usize K_MASK = Capacity - 1;

    alignas(CACHE_LINE_SIZE) std::atomic<usize> m_write_pos{0};
    alignas(CACHE_LINE_SIZE) std::atomic<usize> m_read_pos{0};

    alignas(T) u8 m_slots[Capacity * sizeof(T)];
  };
} // namespace au::containers

export namespace au
{
  // D-008: the default error type is the structured au::Error. ResultT<T, E>
  // stays fully generic for callers that want their own E.
  template<typename T> using Result = ResultT<T, Error>;

  template<typename... Args> [[nodiscard]] inline auto fail(StringView fmt, Args &&...args)
  {
    return fail(String::format(fmt, std::forward<Args>(args)...));
  }
} // namespace au

export namespace au::containers
{
  template<typename T> using Result = ::au::ResultT<T, ::au::Error>;
}

template<>
struct std::formatter<au::containers::BasicString<au::memory::HeapAllocator>>
{
  template<class ParseContext> constexpr auto parse(ParseContext &ctx)
  {
    return ctx.begin();
  }

  // Generic over the context type: std::println/format_to instantiate
  // contexts other than std::format_context (observed: libc++ 20), and a
  // formatter pinned to one context type is invisible to them.
  template<class FormatContext>
  [[nodiscard]] auto format(const au::containers::BasicString<au::memory::HeapAllocator> &s,
                            FormatContext &ctx) const
  {
    const std::string_view view{s.data(), s.size()};
    return std::copy(view.begin(), view.end(), ctx.out());
  }
};

template<>
struct std::formatter<au::StringView>
{
  template<class ParseContext> constexpr auto parse(ParseContext &ctx)
  {
    return ctx.begin();
  }

  template<class FormatContext>
  [[nodiscard]] auto format(const au::StringView &sv, FormatContext &ctx) const
  {
    const std::string_view view{sv.data(), sv.size()};
    return std::copy(view.begin(), view.end(), ctx.out());
  }
};
