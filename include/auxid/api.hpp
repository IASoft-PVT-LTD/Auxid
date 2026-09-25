// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#pragma once

#if defined(AUXID_SHARED_BUILD)
#  if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(AUXID_BUILDING_LIBRARY)
#      define AUXID_API __declspec(dllexport)
#    else
#      define AUXID_API __declspec(dllimport)
#    endif
#  elif defined(AUXID_BUILDING_LIBRARY)
#    define AUXID_API __attribute__((visibility("default")))
#  else
#    define AUXID_API
#  endif
#else
#  define AUXID_API
#endif
