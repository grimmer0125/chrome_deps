// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TIME_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_TIME_UTIL_H_

#include <string>

#include "base/string_piece.h"

class Profile;

namespace base {
class Time;
}  // namespace base

namespace google_apis {
namespace util {

// Parses an RFC 3339 date/time into a base::Time, returning true on success.
// The time string must be in the format "yyyy-mm-ddThh:mm:ss.dddTZ" (TZ is
// either '+hh:mm', '-hh:mm', 'Z' (representing UTC), or an empty string).
bool GetTimeFromString(const base::StringPiece& raw_value, base::Time* time);

// Formats a base::Time as an RFC 3339 date/time (in UTC).
std::string FormatTimeAsString(const base::Time& time);
// Formats a base::Time as an RFC 3339 date/time (in localtime).
std::string FormatTimeAsStringLocaltime(const base::Time& time);

}  // namespace util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TIME_UTIL_H_
