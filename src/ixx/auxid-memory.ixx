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

#include <atomic>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#if defined(AUXID_USE_SYSTEM_MALLOC)
#  if defined(_WIN32)
#    include <malloc.h>
#  endif
#else
#  include <rpmalloc/rpmalloc.h>
#endif

export module auxid.memory;

export import auxid.core;

export namespace au::memory
{
  struct HeapAllocator;

  template<typename T>
  concept AllocatorType = requires(T v, void *ptr, usize size, usize align) {
    { v.alloc(size) } -> std::same_as<void *>;
    { v.alloc(size, align) } -> std::same_as<void *>;
    { v.realloc(ptr, size, size, align) } -> std::same_as<void *>;
    { v.free(ptr, size, align) } -> std::same_as<void>;
  };

  // An allocator that containers may hold by value and copy freely: every copy
  // must allocate from the same place. Stateless allocators (HeapAllocator)
  // qualify directly; stateful ones such as ArenaAllocator are non-copyable and
  // are shared through an AllocatorRef handle instead. Containers, the std
  // adapter, Box and Arc all take an AllocatorHandle.
  template<typename T>
  concept AllocatorHandle = AllocatorType<T> && std::copy_constructible<T>;

  // A stateful allocator passed directly instead of through an AllocatorRef.
  template<typename T>
  concept UnsharedAllocator = AllocatorType<std::remove_cvref_t<T>> && !AllocatorHandle<std::remove_cvref_t<T>>;

  namespace detail
  {
    template<typename... Args> inline constexpr bool LEADS_WITH_UNSHARED_ALLOCATOR = false;

    template<typename First, typename... Rest>
    inline constexpr bool LEADS_WITH_UNSHARED_ALLOCATOR<First, Rest...> = UnsharedAllocator<First>;
  } // namespace detail

  // Satisfied unless the first argument is an UnsharedAllocator. The make_*
  // overloads that take no allocator require this, so make_box<T>(arena) is an
  // error instead of a heap allocation that passes the arena to T.
  template<typename... Args>
  concept NoLeadingUnsharedAllocator = !detail::LEADS_WITH_UNSHARED_ALLOCATOR<Args...>;

  // True when memory from one handle may be freed or reused through the other.
  // Handles that cannot be compared are assumed to differ.
  template<AllocatorHandle A> [[nodiscard]] constexpr auto same_allocator(const A &a, const A &b) noexcept -> bool
  {
    if constexpr (std::is_empty_v<A>)
      return true;
    else if constexpr (std::equality_comparable<A>)
      return a == b;
    else
      return false;
  }

  template<typename T, AllocatorHandle A = HeapAllocator> class StdAllocatorAdapter
  {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::bool_constant<std::is_empty_v<A>>;

    constexpr StdAllocatorAdapter() noexcept
      requires std::default_initializable<A>
    = default;

    // Implicit, so a Vec can be built straight from a handle: Vec<T, A> v(handle).
    constexpr StdAllocatorAdapter(A alloc) noexcept : m_alloc(alloc)
    {
    }

    template<typename U>
    constexpr StdAllocatorAdapter(const StdAllocatorAdapter<U, A> &other) noexcept : m_alloc(other.inner())
    {
    }

    [[nodiscard]] constexpr auto select_on_container_copy_construction() const noexcept -> StdAllocatorAdapter
    {
      return *this;
    }

    [[nodiscard]] auto allocate(size_type n) -> T *
    {
      if (n > (static_cast<size_type>(-1) / sizeof(T)))
        panic_at("StdAllocatorAdapter::allocate: byte count overflows", __FILE__, __LINE__);
      return static_cast<T *>(m_alloc.alloc(n * sizeof(T), alignof(T)));
    }

    auto deallocate(T *p, size_type n) noexcept -> void
    {
      m_alloc.free(p, n * sizeof(T), alignof(T));
    }

    template<typename U> [[nodiscard]] auto operator==(const StdAllocatorAdapter<U, A> &other) const noexcept -> bool
    {
      if constexpr (std::is_empty_v<A>)
        return true;
      else
        return m_alloc == other.inner();
    }

    [[nodiscard]] auto inner() const noexcept -> const A &
    {
      return m_alloc;
    }

private:
    AUXID_NO_UNIQUE_ADDRESS A m_alloc{};
  };
} // namespace au::memory

export namespace au::memory
{
#if !defined(AUXID_USE_SYSTEM_MALLOC)
  inline constexpr usize RPMALLOC_NATURAL_ALIGN = 16;
#endif

  // Foreign-thread guarantee (D-006): allocation is safe from ANY thread,
  // including threads the Auxid thread guards never touched (web-engine
  // callbacks, OS thread-pool completions, third-party library threads).
  // rpmalloc lazily creates the calling thread's heap on first allocation and
  // finalizes it via an FLS/pthread-key destructor on thread exit; frees of
  // blocks owned by another thread's heap are routed through atomic deferred
  // lists. The thread guards remain the normal path — they also own the
  // per-thread logger — lazy init is the safety net, not the design.
  // Precondition: rpmalloc_initialize has run (the MainThreadGuard's job).
  struct HeapAllocator
  {
#if defined(AUXID_USE_SYSTEM_MALLOC)
    static constexpr usize natural_align = alignof(std::max_align_t);
#else
    static constexpr usize natural_align = RPMALLOC_NATURAL_ALIGN;
#endif

#if defined(AUXID_USE_SYSTEM_MALLOC)
private:
    static constexpr usize SYSTEM_MALLOC_NATURAL_ALIGN = alignof(std::max_align_t);

    [[nodiscard]] static inline auto sys_aligned_alloc(usize size, usize align) -> void *
    {
#  if defined(_WIN32)
      return ::_aligned_malloc(size, align);
#  else
      const usize aligned_size = (size + align - 1) & ~(align - 1);
      return std::aligned_alloc(align, aligned_size);
#  endif
    }

    static inline auto sys_aligned_free(void *ptr) -> void
    {
#  if defined(_WIN32)
      ::_aligned_free(ptr);
#  else
      std::free(ptr);
#  endif
    }

public:
    [[nodiscard]] inline auto alloc(usize size) -> void *
    {
      void *p = std::malloc(size);
      if (!p)
        panic_at("HeapAllocator::alloc: out of memory", __FILE__, __LINE__);
      return p;
    }

    [[nodiscard]] inline auto alloc(usize size, usize align) -> void *
    {
      if (align <= SYSTEM_MALLOC_NATURAL_ALIGN)
        return alloc(size);
      void *p = sys_aligned_alloc(size, align);
      if (!p)
        panic_at("HeapAllocator::alloc: out of memory", __FILE__, __LINE__);
      return p;
    }

    [[nodiscard]] inline auto try_alloc(usize size) -> void *
    {
      return std::malloc(size);
    }

    [[nodiscard]] inline auto try_alloc(usize size, usize align) -> void *
    {
      if (align <= SYSTEM_MALLOC_NATURAL_ALIGN)
        return std::malloc(size);
      return sys_aligned_alloc(size, align);
    }

    [[nodiscard]] inline auto realloc(void *ptr, usize old_size, usize new_size, usize align) -> void *
    {
      if (align <= SYSTEM_MALLOC_NATURAL_ALIGN)
      {
        void *p = std::realloc(ptr, new_size);
        if (!p)
          panic_at("HeapAllocator::realloc: out of memory", __FILE__, __LINE__);
        return p;
      }
      void *fresh = alloc(new_size, align);
      if (ptr)
      {
        std::memcpy(fresh, ptr, old_size < new_size ? old_size : new_size);
        sys_aligned_free(ptr);
      }
      return fresh;
    }

    inline auto free(void *ptr, usize size, usize align) -> void
    {
      AU_UNUSED(size);
      if (align <= SYSTEM_MALLOC_NATURAL_ALIGN)
        std::free(ptr);
      else
        sys_aligned_free(ptr);
    }
#else
    [[nodiscard]] inline auto alloc(usize size) -> void *
    {
      void *p = ::rpmalloc(size);
      if (!p)
        panic_at("HeapAllocator::alloc: out of memory", __FILE__, __LINE__);
      return p;
    }

    [[nodiscard]] inline auto alloc(usize size, usize align) -> void *
    {
      void *p = (align <= RPMALLOC_NATURAL_ALIGN) ? ::rpmalloc(size) : ::rpaligned_alloc(align, size);
      if (!p)
        panic_at("HeapAllocator::alloc: out of memory", __FILE__, __LINE__);
      return p;
    }

    [[nodiscard]] inline auto try_alloc(usize size) -> void *
    {
      return ::rpmalloc(size);
    }

    [[nodiscard]] inline auto try_alloc(usize size, usize align) -> void *
    {
      return (align <= RPMALLOC_NATURAL_ALIGN) ? ::rpmalloc(size) : ::rpaligned_alloc(align, size);
    }

    [[nodiscard]] inline auto realloc(void *ptr, usize old_size, usize new_size, usize align) -> void *
    {
      void *p = ::rpaligned_realloc(ptr, align, new_size, old_size, 0);
      if (!p)
        panic_at("HeapAllocator::realloc: out of memory", __FILE__, __LINE__);
      return p;
    }

    inline auto free(void *ptr, usize size, usize align) -> void
    {
      AU_UNUSED(size);
      AU_UNUSED(align);
      ::rpfree(ptr);
    }
#endif
  };

  static_assert(AllocatorType<HeapAllocator>, "Allocator class must conform to AllocatorT");
} // namespace au::memory

export namespace au::memory
{
  // The bump state (buffer, length, offset) lives inside the arena, so the arena
  // is neither copyable nor movable: a copy would hand out memory the original
  // already handed out, and a move would detach every AllocatorRef bound to the
  // original. To give an arena to a container (Vec, BasicString, ...) or to
  // make_box / make_arc, pass AllocatorRef<ArenaAllocator>.
  //
  // Debug builds count the live AllocatorRef handles bound to the arena.
  // Resetting the arena (clear, init) or destroying it while a handle is alive
  // panics: a container holding the handle (even an empty one) could otherwise
  // hand out or keep memory that the reset arena will give away again. Handles
  // stored inside the arena's own buffer are not counted: objects built in the
  // arena may be dropped by a reset without running their destructors. The
  // count is atomic because handles, unlike the arena itself, may be copied and
  // destroyed on any thread. Release builds carry no count and AllocatorRef
  // stays a plain pointer.
  struct ArenaAllocator
  {
    u8 *buffer = nullptr;
    usize length = 0;
    usize offset = 0;

    ArenaAllocator() noexcept = default;

    ArenaAllocator(const ArenaAllocator &) = delete;
    auto operator=(const ArenaAllocator &) -> ArenaAllocator & = delete;
    ArenaAllocator(ArenaAllocator &&) = delete;
    auto operator=(ArenaAllocator &&) -> ArenaAllocator & = delete;

#if !defined(NDEBUG)
    ~ArenaAllocator()
    {
      if (debug_live_handles() != 0)
        panic_at("ArenaAllocator destroyed while AllocatorRef handles are still alive", __FILE__, __LINE__);
    }

    // Returns whether the handle at `handle` was counted; only a counted handle
    // is released.
    [[nodiscard]] auto debug_retain_handle(const void *handle) noexcept -> bool
    {
      const uintptr_t base = reinterpret_cast<uintptr_t>(buffer);
      const uintptr_t addr = reinterpret_cast<uintptr_t>(handle);
      if (addr >= base && addr - base < length)
        return false;
      m_live_handles.fetch_add(1, std::memory_order_relaxed);
      return true;
    }

    auto debug_release_handle() noexcept -> void
    {
      m_live_handles.fetch_sub(1, std::memory_order_release);
    }

    [[nodiscard]] auto debug_live_handles() const noexcept -> usize
    {
      return m_live_handles.load(std::memory_order_acquire);
    }
#endif

    auto init(u8 *buf, usize len) -> void
    {
#if !defined(NDEBUG)
      if (debug_live_handles() != 0)
        panic_at("ArenaAllocator::init: AllocatorRef handles are still alive", __FILE__, __LINE__);
#endif
      buffer = buf;
      length = len;
      offset = 0;
    }

    [[nodiscard]] inline auto alloc(usize size) -> void *
    {
      return alloc(size, 8);
    }

    [[nodiscard]] inline auto alloc(usize size, usize align) -> void *
    {
      void *p = try_alloc(size, align);
      if (!p)
        panic_at("ArenaAllocator::alloc: arena exhausted", __FILE__, __LINE__);
      return p;
    }

    [[nodiscard]] inline auto try_alloc(usize size) -> void *
    {
      return try_alloc(size, 8);
    }

    [[nodiscard]] inline auto try_alloc(usize size, usize align) -> void *
    {
      uintptr_t curr_addr = reinterpret_cast<uintptr_t>(buffer) + offset;
      uintptr_t misalignment = curr_addr & (align - 1);
      usize padding = (align - misalignment) & (align - 1);

      if (offset > length || padding > length - offset || size > length - offset - padding)
        return nullptr;

      void *ptr = buffer + offset + padding;
      offset += padding + size;
      return ptr;
    }

    // The most recent allocation ends exactly at `offset`, so it grows or shrinks
    // in place when the buffer has room. Any other block shrinks in place (its
    // tail is not reclaimed) and is copied into a fresh allocation to grow; the
    // old block stays where it is (arenas never free individually). A block not
    // aligned to `align` is always copied. Panics on exhaustion, like alloc().
    [[nodiscard]] inline auto realloc(void *ptr, usize old_size, usize new_size, usize align) -> void *
    {
      if (!ptr)
        return alloc(new_size, align);

      const uintptr_t base = reinterpret_cast<uintptr_t>(buffer);
      const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
      const bool is_last = addr >= base && addr + old_size == base + offset;
      if ((addr & (align - 1)) == 0)
      {
        if (is_last)
        {
          const usize start = static_cast<usize>(addr - base);
          if (new_size <= length - start)
          {
            offset = start + new_size;
            return ptr;
          }
        }
        else if (new_size <= old_size)
          return ptr;
      }

      void *fresh = alloc(new_size, align);
      std::memcpy(fresh, ptr, old_size < new_size ? old_size : new_size);
      return fresh;
    }

    inline auto free(void *ptr, usize size, usize align) -> void
    {
      AU_UNUSED(ptr);
      AU_UNUSED(size);
      AU_UNUSED(align);
    }

    inline auto clear() -> void
    {
#if !defined(NDEBUG)
      if (debug_live_handles() != 0)
        panic_at("ArenaAllocator::clear: AllocatorRef handles are still alive", __FILE__, __LINE__);
#endif
      offset = 0;
    }

    [[nodiscard]] friend auto operator==(const ArenaAllocator &a, const ArenaAllocator &b) noexcept -> bool
    {
      return a.buffer == b.buffer;
    }

#if !defined(NDEBUG)
private:
    std::atomic<usize> m_live_handles{0};
#endif
  };

  static_assert(AllocatorType<ArenaAllocator>, "Allocator class must conform to AllocatorT");
} // namespace au::memory

export namespace au::memory
{
  // Non-owning, copyable handle to a stateful allocator. Every copy points at
  // the same allocator, so containers that copy their allocator (Vec through
  // StdAllocatorAdapter, BasicString, Box, Arc) all draw from one shared state.
  // Two handles compare equal when they point at the same allocator. The
  // referenced allocator must outlive every handle and every allocation made
  // through it. There is no default (null) handle. In debug builds the handle
  // registers its address with allocators that track handles (ArenaAllocator),
  // so it must not be relocated bytewise; in release builds it is a trivially
  // copyable pointer.
  template<AllocatorType A> class AllocatorRef
  {
public:
    using allocator_type = A;

#if defined(NDEBUG)
    constexpr explicit AllocatorRef(A &alloc) noexcept : m_alloc(&alloc)
    {
    }
#else
    explicit AllocatorRef(A &alloc) noexcept : m_alloc(&alloc)
    {
      retain();
    }

    AllocatorRef(const AllocatorRef &other) noexcept : m_alloc(other.m_alloc)
    {
      retain();
    }

    auto operator=(const AllocatorRef &other) noexcept -> AllocatorRef &
    {
      if (m_alloc != other.m_alloc)
      {
        release();
        m_alloc = other.m_alloc;
        retain();
      }
      return *this;
    }

    ~AllocatorRef()
    {
      release();
    }
#endif

    [[nodiscard]] inline auto alloc(usize size) const -> void *
    {
      return m_alloc->alloc(size);
    }

    [[nodiscard]] inline auto alloc(usize size, usize align) const -> void *
    {
      return m_alloc->alloc(size, align);
    }

    [[nodiscard]] inline auto try_alloc(usize size) const -> void *
      requires requires(A &a, usize s) {
        { a.try_alloc(s) } -> std::same_as<void *>;
      }
    {
      return m_alloc->try_alloc(size);
    }

    [[nodiscard]] inline auto try_alloc(usize size, usize align) const -> void *
      requires requires(A &a, usize s) {
        { a.try_alloc(s, s) } -> std::same_as<void *>;
      }
    {
      return m_alloc->try_alloc(size, align);
    }

    [[nodiscard]] inline auto realloc(void *ptr, usize old_size, usize new_size, usize align) const -> void *
    {
      return m_alloc->realloc(ptr, old_size, new_size, align);
    }

    inline auto free(void *ptr, usize size, usize align) const -> void
    {
      m_alloc->free(ptr, size, align);
    }

    [[nodiscard]] constexpr auto get() const noexcept -> A &
    {
      return *m_alloc;
    }

    [[nodiscard]] friend constexpr auto operator==(const AllocatorRef &a, const AllocatorRef &b) noexcept -> bool
    {
      return a.m_alloc == b.m_alloc;
    }

private:
#if !defined(NDEBUG)
    static constexpr bool TRACKS_HANDLES = requires(A &a, const void *handle) {
      { a.debug_retain_handle(handle) } -> std::same_as<bool>;
      a.debug_release_handle();
    };

    auto retain() noexcept -> void
    {
      if constexpr (TRACKS_HANDLES)
        m_counted = m_alloc->debug_retain_handle(this);
    }

    auto release() noexcept -> void
    {
      if constexpr (TRACKS_HANDLES)
      {
        if (m_counted)
          m_alloc->debug_release_handle();
        m_counted = false;
      }
    }

    bool m_counted = false;
#endif

    A *m_alloc;
  };

  static_assert(AllocatorHandle<AllocatorRef<ArenaAllocator>>, "AllocatorRef must be a copyable allocator");
  static_assert(!AllocatorHandle<ArenaAllocator>, "ArenaAllocator must only be shared through AllocatorRef");
#if defined(NDEBUG)
  static_assert(std::is_trivially_copyable_v<AllocatorRef<ArenaAllocator>> &&
                    sizeof(AllocatorRef<ArenaAllocator>) == sizeof(ArenaAllocator *),
                "AllocatorRef must be a plain pointer in release builds");
#endif
} // namespace au::memory

export namespace au::memory
{
  template<typename T, typename Allocator = HeapAllocator> struct AuxidDeleter
  {
    AUXID_NO_UNIQUE_ADDRESS Allocator m_alloc{};

    constexpr AuxidDeleter() noexcept
      requires std::default_initializable<Allocator>
    = default;

    constexpr explicit AuxidDeleter(Allocator alloc) noexcept : m_alloc(std::move(alloc))
    {
    }

    constexpr auto operator()(T *ptr) const noexcept -> void
    {
      if (ptr)
      {
        ptr->~T();
        const_cast<AuxidDeleter *>(this)->m_alloc.free(ptr, sizeof(T), alignof(T));
      }
    }
  };

  template<typename T, typename Allocator> using BoxAllocatorDeleter = AuxidDeleter<T, Allocator>;

  template<typename T, typename Deleter = AuxidDeleter<T, HeapAllocator>> using Box = std::unique_ptr<T, Deleter>;

  template<typename T, AllocatorHandle Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_box(Allocator alloc, Args &&...args) -> Box<T, AuxidDeleter<T, Allocator>>
  {
    void *mem = alloc.alloc(sizeof(T), alignof(T));
    if (!mem)
      panic_at("make_box: allocation failed", __FILE__, __LINE__);
    T *ptr = new (mem) T(std::forward<Args>(args)...);
    return Box<T, AuxidDeleter<T, Allocator>>(ptr, AuxidDeleter<T, Allocator>(alloc));
  }

  template<typename T, typename... Args>
    requires NoLeadingUnsharedAllocator<Args...>
  [[nodiscard]] auto make_box(Args &&...args) -> Box<T, AuxidDeleter<T, HeapAllocator>>
  {
    return make_box<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }

  template<typename T, AllocatorHandle Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_box_protected(Allocator alloc, Args &&...args) -> Box<T, AuxidDeleter<T, Allocator>>
  {
    struct Enabler : public T
    {
      explicit Enabler(Args &&...a) : T(std::forward<Args>(a)...)
      {
      }
    };

    void *mem = alloc.alloc(sizeof(T), alignof(T));
    if (!mem)
      panic_at("make_box_protected: allocation failed", __FILE__, __LINE__);
    T *ptr = new (mem) Enabler(std::forward<Args>(args)...);
    return Box<T, AuxidDeleter<T, Allocator>>(ptr, AuxidDeleter<T, Allocator>(alloc));
  }

  template<typename T, typename... Args>
    requires NoLeadingUnsharedAllocator<Args...>
  [[nodiscard]] auto make_box_protected(Args &&...args) -> Box<T, AuxidDeleter<T, HeapAllocator>>
  {
    return make_box_protected<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }
} // namespace au::memory

export namespace au::memory
{
  template<typename T, AllocatorHandle Allocator = HeapAllocator> class Arc
  {
public:
    struct ControlBlock
    {
      std::atomic<u32> refs;
      T storage;

      template<typename... Args> explicit ControlBlock(Args &&...args) : refs(1), storage(std::forward<Args>(args)...)
      {
      }
    };

    constexpr Arc() noexcept
      requires std::default_initializable<Allocator>
    = default;

    ~Arc()
    {
      release();
    }

    Arc(const Arc &other) noexcept : m_block(other.m_block), m_alloc(other.m_alloc)
    {
      retain();
    }

    Arc(Arc &&other) noexcept : m_block(other.m_block), m_alloc(other.m_alloc)
    {
      other.m_block = nullptr;
    }

    auto operator=(const Arc &other) noexcept -> Arc &
    {
      if (this != &other)
        replace(Arc(other));
      return *this;
    }

    auto operator=(Arc &&other) noexcept -> Arc &
    {
      if (this != &other)
        replace(Arc(std::move(other)));
      return *this;
    }

    [[nodiscard]] auto get() const noexcept -> T *
    {
      return m_block ? &m_block->storage : nullptr;
    }

    [[nodiscard]] auto operator->() const noexcept -> T *
    {
      return &m_block->storage;
    }

    [[nodiscard]] auto operator*() const noexcept -> T &
    {
      return m_block->storage;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
      return m_block != nullptr;
    }

    [[nodiscard]] auto use_count() const noexcept -> u32
    {
      return m_block ? m_block->refs.load(std::memory_order_relaxed) : 0;
    }

    auto reset() noexcept -> void
    {
      release();
      m_block = nullptr;
    }

    [[nodiscard]] auto operator==(const Arc &rhs) const noexcept -> bool
    {
      return m_block == rhs.m_block;
    }

    [[nodiscard]] auto operator==(decltype(nullptr)) const noexcept -> bool
    {
      return m_block == nullptr;
    }

    constexpr Arc(ControlBlock *cb, Allocator alloc) noexcept : m_block(cb), m_alloc(alloc)
    {
    }

private:
    // `incoming` is a separate object and the old block is released last: the
    // assignment source may live inside that block (node = node->next).
    auto replace(Arc &&incoming) noexcept -> void
    {
      const Arc outgoing(std::move(*this));
      m_block = incoming.m_block;
      m_alloc = incoming.m_alloc;
      incoming.m_block = nullptr;
    }

    auto retain() const noexcept -> void
    {
      if (m_block)
        m_block->refs.fetch_add(1, std::memory_order_relaxed);
    }

    auto release() noexcept -> void
    {
      if (!m_block)
        return;
      if (m_block->refs.fetch_sub(1, std::memory_order_release) == 1)
      {
        std::atomic_thread_fence(std::memory_order_acquire);
        m_block->~ControlBlock();
        m_alloc.free(m_block, sizeof(ControlBlock), alignof(ControlBlock));
      }
    }

    ControlBlock *m_block = nullptr;
    AUXID_NO_UNIQUE_ADDRESS Allocator m_alloc{};
  };

  template<typename T, AllocatorHandle Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_arc(Allocator alloc, Args &&...args) -> Arc<T, Allocator>
  {
    using CB = typename Arc<T, Allocator>::ControlBlock;
    void *mem = alloc.alloc(sizeof(CB), alignof(CB));
    if (!mem)
      panic_at("make_arc: allocation failed", __FILE__, __LINE__);
    auto *cb = new (mem) CB(std::forward<Args>(args)...);
    return Arc<T, Allocator>(cb, alloc);
  }

  template<typename T, typename... Args>
    requires NoLeadingUnsharedAllocator<Args...>
  [[nodiscard]] auto make_arc(Args &&...args) -> Arc<T, HeapAllocator>
  {
    return make_arc<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }

  // Known limitation: unlike make_box_protected (which placement-constructs the
  // Enabler directly), this constructs T's ControlBlock storage FROM an Enabler
  // temporary — so T additionally needs an accessible copy/move constructor.
  template<typename T, AllocatorHandle Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_arc_protected(Allocator alloc, Args &&...args) -> Arc<T, Allocator>
  {
    struct Enabler : public T
    {
      explicit Enabler(Args &&...a) : T(std::forward<Args>(a)...)
      {
      }
    };

    using CB = typename Arc<T, Allocator>::ControlBlock;
    void *mem = alloc.alloc(sizeof(CB), alignof(CB));
    if (!mem)
      panic_at("make_arc_protected: allocation failed", __FILE__, __LINE__);
    auto *cb = new (mem) CB(Enabler(std::forward<Args>(args)...));
    return Arc<T, Allocator>(cb, alloc);
  }

  template<typename T, typename... Args>
    requires NoLeadingUnsharedAllocator<Args...>
  [[nodiscard]] auto make_arc_protected(Args &&...args) -> Arc<T, HeapAllocator>
  {
    return make_arc_protected<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }

  class RefCounted
  {
public:
    constexpr RefCounted() noexcept = default;

    RefCounted(const RefCounted &) noexcept : m_refs(0)
    {
    }

    auto operator=(const RefCounted &) noexcept -> RefCounted &
    {
      return *this;
    }

    auto arc_retain() const noexcept -> void
    {
      m_refs.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] auto arc_release() const noexcept -> bool
    {
      if (m_refs.fetch_sub(1, std::memory_order_release) == 1)
      {
        std::atomic_thread_fence(std::memory_order_acquire);
        return true;
      }
      return false;
    }

    [[nodiscard]] auto arc_count() const noexcept -> u32
    {
      return m_refs.load(std::memory_order_relaxed);
    }

private:
    mutable std::atomic<u32> m_refs{0};
  };

  template<typename T, AllocatorHandle Allocator = HeapAllocator> class IntrusiveArc
  {
    static_assert(std::is_base_of_v<RefCounted, T>, "IntrusiveArc<T> requires T : RefCounted");

public:
    constexpr IntrusiveArc() noexcept
      requires std::default_initializable<Allocator>
    = default;

    explicit IntrusiveArc(T *ptr) noexcept
      requires std::default_initializable<Allocator>
        : IntrusiveArc(ptr, Allocator{})
    {
    }

    explicit IntrusiveArc(T *ptr, Allocator alloc) noexcept : m_ptr(ptr), m_alloc(alloc)
    {
      if (m_ptr)
        m_ptr->arc_retain();
    }

    ~IntrusiveArc()
    {
      release();
    }

    IntrusiveArc(const IntrusiveArc &o) noexcept : m_ptr(o.m_ptr), m_alloc(o.m_alloc)
    {
      if (m_ptr)
        m_ptr->arc_retain();
    }

    IntrusiveArc(IntrusiveArc &&o) noexcept : m_ptr(o.m_ptr), m_alloc(o.m_alloc)
    {
      o.m_ptr = nullptr;
    }

    auto operator=(const IntrusiveArc &o) noexcept -> IntrusiveArc &
    {
      if (this != &o)
        replace(IntrusiveArc(o));
      return *this;
    }

    auto operator=(IntrusiveArc &&o) noexcept -> IntrusiveArc &
    {
      if (this != &o)
        replace(IntrusiveArc(std::move(o)));
      return *this;
    }

    [[nodiscard]] auto get() const noexcept -> T *
    {
      return m_ptr;
    }

    [[nodiscard]] auto operator->() const noexcept -> T *
    {
      return m_ptr;
    }

    [[nodiscard]] auto operator*() const noexcept -> T &
    {
      return *m_ptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
      return m_ptr != nullptr;
    }

    [[nodiscard]] auto use_count() const noexcept -> u32
    {
      return m_ptr ? m_ptr->arc_count() : 0;
    }

    auto reset() noexcept -> void
    {
      release();
      m_ptr = nullptr;
    }

    [[nodiscard]] auto operator==(const IntrusiveArc &rhs) const noexcept -> bool
    {
      return m_ptr == rhs.m_ptr;
    }

    [[nodiscard]] auto operator==(decltype(nullptr)) const noexcept -> bool
    {
      return m_ptr == nullptr;
    }

private:
    // Same ordering as Arc::replace.
    auto replace(IntrusiveArc &&incoming) noexcept -> void
    {
      const IntrusiveArc outgoing(std::move(*this));
      m_ptr = incoming.m_ptr;
      m_alloc = incoming.m_alloc;
      incoming.m_ptr = nullptr;
    }

    auto release() noexcept -> void
    {
      if (m_ptr && m_ptr->arc_release())
      {
        m_ptr->~T();
        m_alloc.free(m_ptr, sizeof(T), alignof(T));
      }
    }

    T *m_ptr = nullptr;
    AUXID_NO_UNIQUE_ADDRESS Allocator m_alloc{};
  };

  template<typename T, AllocatorHandle Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_intrusive_arc(Allocator alloc, Args &&...args) -> IntrusiveArc<T, Allocator>
  {
    void *mem = alloc.alloc(sizeof(T), alignof(T));
    if (!mem)
      panic_at("make_intrusive_arc: allocation failed", __FILE__, __LINE__);
    auto *p = new (mem) T(std::forward<Args>(args)...);
    return IntrusiveArc<T, Allocator>(p, alloc);
  }

  template<typename T, typename... Args>
    requires NoLeadingUnsharedAllocator<Args...>
  [[nodiscard]] auto make_intrusive_arc(Args &&...args) -> IntrusiveArc<T, HeapAllocator>
  {
    return make_intrusive_arc<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }
} // namespace au::memory

export namespace au
{
  using ::au::memory::Arc;
  using ::au::memory::IntrusiveArc;
  using ::au::memory::make_arc;
  using ::au::memory::make_arc_protected;
  using ::au::memory::make_intrusive_arc;
  using ::au::memory::RefCounted;

  using ::au::memory::AuxidDeleter;
  using ::au::memory::Box;
  using ::au::memory::BoxAllocatorDeleter;
  using ::au::memory::make_box;
  using ::au::memory::make_box_protected;
} // namespace au
