// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Non-owning type-erased callable reference (std::function_ref-style).
// Zero heap allocations; binds to any invocable lvalue or temporary for
// the duration of a call.
#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace ckv {

template <typename Signature>
class FunctionRef;

// Non-owning type-erased callable reference for signature `Ret(Args...)`.
template <typename Ret, typename... Args>
class FunctionRef<Ret(Args...)> {
public:
    constexpr FunctionRef() noexcept = default;
    constexpr FunctionRef(std::nullptr_t) noexcept {}

    template <typename F,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, FunctionRef> &&
                                          std::is_invocable_r_v<Ret, F&&, Args...>>>
    constexpr FunctionRef(F&& fn) noexcept
        : obj_(const_cast<void*>(static_cast<const void*>(std::addressof(fn)))),
          invoker_([](void* obj, Args... args) -> Ret {
              return static_cast<Ret>(
                  (*static_cast<std::remove_reference_t<F>*>(obj))(std::forward<Args>(args)...));
          }) {}

    constexpr explicit operator bool() const noexcept { return invoker_ != nullptr; }

    Ret operator()(Args... args) const {
        return invoker_(obj_, std::forward<Args>(args)...);
    }

private:
    void* obj_ = nullptr;
    Ret (*invoker_)(void*, Args...) = nullptr;
};

}  // namespace ckv
