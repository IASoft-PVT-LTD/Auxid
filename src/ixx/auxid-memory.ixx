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

  template<typename T, AllocatorType A = HeapAllocator> class StdAllocatorAdapter
  {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::bool_constant<std::is_empty_v<A>>;

    constexpr StdAllocatorAdapter() noexcept = default;

    constexpr explicit StdAllocatorAdapter(A alloc) noexcept : m_alloc(alloc)
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
  struct ArenaAllocator
  {
    u8 *buffer = nullptr;
    usize length = 0;
    usize offset = 0;

    auto init(u8 *buf, usize len) -> void
    {
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
      ptrdiff_t padding = (align - misalignment) & (align - 1);

      usize total = size + padding;

      if (offset > length || total > length - offset)
        return nullptr;

      void *ptr = buffer + offset + padding;
      offset += total;
      return ptr;
    }

    [[nodiscard]] inline auto realloc(void *ptr, usize old_size, usize new_size, usize align) -> void *
    {
      AU_UNUSED(ptr);
      AU_UNUSED(old_size);
      AU_UNUSED(new_size);
      AU_UNUSED(align);
      panic_at("ArenaAllocator::realloc: not supported", __FILE__, __LINE__);
      return nullptr;
    }

    inline auto free(void *ptr, usize size, usize align) -> void
    {
      AU_UNUSED(ptr);
      AU_UNUSED(size);
      AU_UNUSED(align);
    }

    inline auto clear() -> void
    {
      offset = 0;
    }

    [[nodiscard]] friend auto operator==(const ArenaAllocator &a, const ArenaAllocator &b) noexcept -> bool
    {
      return a.buffer == b.buffer;
    }
  };

  static_assert(AllocatorType<ArenaAllocator>, "Allocator class must conform to AllocatorT");
} // namespace au::memory

export namespace au::memory
{
  template<typename T, typename Allocator = HeapAllocator> struct AuxidDeleter
  {
    AUXID_NO_UNIQUE_ADDRESS Allocator m_alloc{};

    constexpr AuxidDeleter() noexcept = default;

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

  template<typename T, AllocatorType Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_box(Allocator alloc, Args &&...args) -> Box<T, AuxidDeleter<T, Allocator>>
  {
    void *mem = alloc.alloc(sizeof(T), alignof(T));
    if (!mem)
      panic_at("make_box: allocation failed", __FILE__, __LINE__);
    T *ptr = new (mem) T(std::forward<Args>(args)...);
    return Box<T, AuxidDeleter<T, Allocator>>(ptr, AuxidDeleter<T, Allocator>(alloc));
  }

  template<typename T, typename... Args>
  [[nodiscard]] auto make_box(Args &&...args) -> Box<T, AuxidDeleter<T, HeapAllocator>>
  {
    return make_box<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }

  template<typename T, AllocatorType Allocator = HeapAllocator, typename... Args>
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
  [[nodiscard]] auto make_box_protected(Args &&...args) -> Box<T, AuxidDeleter<T, HeapAllocator>>
  {
    return make_box_protected<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }
} // namespace au::memory

export namespace au::memory
{
  template<typename T, AllocatorType Allocator = HeapAllocator> class Arc
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

    constexpr Arc() noexcept = default;

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
      {
        release();
        m_block = other.m_block;
        m_alloc = other.m_alloc;
        retain();
      }
      return *this;
    }

    auto operator=(Arc &&other) noexcept -> Arc &
    {
      if (this != &other)
      {
        release();
        m_block = other.m_block;
        m_alloc = other.m_alloc;
        other.m_block = nullptr;
      }
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

  template<typename T, AllocatorType Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_arc(Allocator alloc, Args &&...args) -> Arc<T, Allocator>
  {
    using CB = typename Arc<T, Allocator>::ControlBlock;
    void *mem = alloc.alloc(sizeof(CB), alignof(CB));
    if (!mem)
      panic_at("make_arc: allocation failed", __FILE__, __LINE__);
    auto *cb = new (mem) CB(std::forward<Args>(args)...);
    return Arc<T, Allocator>(cb, alloc);
  }

  template<typename T, typename... Args> [[nodiscard]] auto make_arc(Args &&...args) -> Arc<T, HeapAllocator>
  {
    return make_arc<T, HeapAllocator>(HeapAllocator{}, std::forward<Args>(args)...);
  }

  // Known limitation: unlike make_box_protected (which placement-constructs the
  // Enabler directly), this constructs T's ControlBlock storage FROM an Enabler
  // temporary — so T additionally needs an accessible copy/move constructor.
  template<typename T, AllocatorType Allocator = HeapAllocator, typename... Args>
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

  template<typename T, typename... Args> [[nodiscard]] auto make_arc_protected(Args &&...args) -> Arc<T, HeapAllocator>
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

  template<typename T, AllocatorType Allocator = HeapAllocator> class IntrusiveArc
  {
    static_assert(std::is_base_of_v<RefCounted, T>, "IntrusiveArc<T> requires T : RefCounted");

public:
    constexpr IntrusiveArc() noexcept = default;

    explicit IntrusiveArc(T *ptr, Allocator alloc = Allocator{}) noexcept : m_ptr(ptr), m_alloc(alloc)
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
      {
        release();
        m_ptr = o.m_ptr;
        m_alloc = o.m_alloc;
        if (m_ptr)
          m_ptr->arc_retain();
      }
      return *this;
    }

    auto operator=(IntrusiveArc &&o) noexcept -> IntrusiveArc &
    {
      if (this != &o)
      {
        release();
        m_ptr = o.m_ptr;
        m_alloc = o.m_alloc;
        o.m_ptr = nullptr;
      }
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

  template<typename T, AllocatorType Allocator = HeapAllocator, typename... Args>
  [[nodiscard]] auto make_intrusive_arc(Allocator alloc, Args &&...args) -> IntrusiveArc<T, Allocator>
  {
    void *mem = alloc.alloc(sizeof(T), alignof(T));
    if (!mem)
      panic_at("make_intrusive_arc: allocation failed", __FILE__, __LINE__);
    auto *p = new (mem) T(std::forward<Args>(args)...);
    return IntrusiveArc<T, Allocator>(p, alloc);
  }

  template<typename T, typename... Args>
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
