/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Compatibility shim for the EmbedLite series. Upstream removed
 * mozilla::Unused (Bug 1994397, Firefox 146); embedding/ still uses
 * `Unused << expr` in ~84 places. Header-only, no MFBT_DATA symbol.
 * `Unused << already_AddRefed<T>` is deliberately deleted: the old friend
 * operator released the reference, a silent leak would be worse than a
 * compile error. */

#ifndef mozilla_Unused_h
#define mozilla_Unused_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"

namespace mozilla {

struct unused_t {
  template <typename T>
  inline void operator<<(const T& /*unused*/) const {}

  template <typename T>
  void operator<<(::already_AddRefed<T>&&) const = delete;
};

inline constexpr unused_t Unused{};

}  // namespace mozilla

#endif  // mozilla_Unused_h
