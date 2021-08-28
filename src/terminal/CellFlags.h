/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <cstdint>

#include <fmt/format.h>

namespace terminal {

enum class CellFlags : uint16_t // TODO(pr) provide fmt::formatter
{
    None = 0,

    Bold = (1 << 0),
    Faint = (1 << 1),
    Italic = (1 << 2),
    Underline = (1 << 3),
    Blinking = (1 << 4),
    Inverse = (1 << 5),
    Hidden = (1 << 6),
    CrossedOut = (1 << 7),
    DoublyUnderlined = (1 << 8),
    CurlyUnderlined = (1 << 9),
    DottedUnderline = (1 << 10),
    DashedUnderline = (1 << 11),
    Framed = (1 << 12),
    Encircled = (1 << 13),
    Overline = (1 << 14),
};

constexpr CellFlags& operator|=(CellFlags& a, CellFlags b) noexcept
{
    a = static_cast<CellFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
	return a;
}

constexpr CellFlags& operator&=(CellFlags& a, CellFlags b) noexcept
{
    a = static_cast<CellFlags>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
	return a;
}

/// Tests if @p b is contained in @p a.
constexpr bool operator&(CellFlags a, CellFlags b) noexcept
{
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}

constexpr bool contains_all(CellFlags _base, CellFlags _test) noexcept
{
    return (static_cast<unsigned>(_base) & static_cast<unsigned>(_test)) == static_cast<unsigned>(_test);
}

/// Merges two CellFlags sets.
constexpr CellFlags operator|(CellFlags a, CellFlags b) noexcept
{
    return static_cast<CellFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

/// Inverts the flags set.
constexpr CellFlags operator~(CellFlags a) noexcept
{
    return static_cast<CellFlags>(~static_cast<unsigned>(a));
}

/// Tests for all flags cleared state.
constexpr bool operator!(CellFlags a) noexcept
{
    return static_cast<unsigned>(a) == 0;
}

}
