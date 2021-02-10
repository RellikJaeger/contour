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

#include <crispy/FNV.h>

#include <fmt/format.h>

#include <string>

namespace text {

struct vec2
{
    int x;
    int y;
};

enum class font_weight
{
    thin,
    normal,
    bold,
    extra_bold,
};

enum class font_slant
{
    normal,
    italic,
    oblique
};

enum class font_spacing
{
    proportional,
    mono
};

struct font_description
{
    std::string familyName;
    font_weight weight = font_weight::normal;
    font_slant slant = font_slant::normal;
    font_spacing spacing = font_spacing::proportional;

    // returns "familyName [weight] [slant]"
    std::string toPattern() const;

    // Parses a font pattern of form "familyName" into a font_description."
    static font_description parse(std::string_view _pattern);
};

inline bool operator==(font_description const& a, font_description const& b)
{
    return a.familyName == b.familyName
        && a.weight == b.weight
        && a.slant == b.slant
        && a.spacing == b.spacing;
}

inline bool operator!=(font_description const& a, font_description const& b)
{
    return !(a == b);
}

struct font_metrics
{
    int line_height;
    int advance;
    int ascender;
    int descender;
    int underline_position;
    int underline_thickness;
};

struct font_size
{
    double pt;
};

constexpr font_size operator+(font_size a, font_size b) noexcept
{
    return font_size{ a.pt + b.pt };
}

constexpr font_size operator-(font_size a, font_size b) noexcept
{
    return font_size{ a.pt - b.pt };
}

constexpr bool operator<(font_size a, font_size b) noexcept
{
    return a.pt < b.pt;
}

struct font_key
{
    unsigned value;
};

constexpr bool operator<(font_key a, font_key b) noexcept
{
    return a.value < b.value;
}

constexpr bool operator==(font_key a, font_key b) noexcept
{
    return a.value == b.value;
}

struct glyph_index
{
    unsigned value;
};

struct glyph_key
{
    font_key font;
    font_size size;
    glyph_index index;
};

constexpr bool operator==(glyph_key const& a, glyph_key const& b) noexcept
{
    return a.font.value == b.font.value
        && a.size.pt == b.size.pt
        && a.index.value == b.index.value;
}

constexpr bool operator<(glyph_key const& a, glyph_key const& b) noexcept
{
    return a.font.value < b.font.value
        || (a.font.value == b.font.value && a.size.pt < b.size.pt)
        || (a.font.value == b.font.value && a.size.pt == b.size.pt && a.index.value < b.index.value);
}

enum class render_mode
{
    bitmap, //!< bitmaps are preferred
    gray,   //!< gray-scale anti-aliasing
    light,  //!< gray-scale anti-aliasing for optimized for LCD screens
    lcd,    //!< LCD-optimized anti-aliasing
    color   //!< embedded color bitmaps are preferred
};

} // end namespace

namespace std { // {{{
    template<>
    struct hash<text::font_key> {
        std::size_t operator()(text::font_key _key) const noexcept
        {
            return _key.value;
        }
    };

    template<>
    struct hash<text::glyph_index> {
        std::size_t operator()(text::glyph_index  _index) const noexcept
        {
            return _index.value;
        }
    };

    template<>
    struct hash<text::glyph_key> {
        std::size_t operator()(text::glyph_key const& _key) const noexcept
        {
            auto const f = _key.font.value;
            auto const i = _key.index.value;
            auto const s = int(_key.size.pt * 10.0);
            return std::size_t(((size_t(f) << 32) & 0xFFFF)
                             | ((i << 16) & 0xFFFF)
                             |  (s & 0xFF));
        }
    };

    template<>
    struct hash<text::font_description> {
        std::size_t operator()(text::font_description const& fd) const noexcept
        {
            auto fnv = crispy::FNV<char>();
            return size_t(
                fnv(fnv(fnv(fnv(fd.familyName), char(fd.weight)), char(fd.slant)), char(fd.spacing))
            );
        }
    };
} // }}}

namespace fmt { // {{{
    template <>
    struct formatter<text::vec2> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::vec2 _value, FormatContext& ctx)
        {
            return format_to(ctx.out(), "({}, {})", _value.x, _value.y);
        }
    };

    template <>
    struct formatter<text::font_weight> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_weight _value, FormatContext& ctx)
        {
            switch (_value)
            {
                case text::font_weight::thin:
                    return format_to(ctx.out(), "Thin");
                case text::font_weight::normal:
                    return format_to(ctx.out(), "Regular");
                case text::font_weight::bold:
                    return format_to(ctx.out(), "Bold");
                case text::font_weight::extra_bold:
                    return format_to(ctx.out(), "Extra Bold");
            }
            return format_to(ctx.out(), "({})", unsigned(_value));
        }
    };

    template <>
    struct formatter<text::font_slant> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_slant _value, FormatContext& ctx)
        {
            switch (_value)
            {
                case text::font_slant::normal:
                    return format_to(ctx.out(), "Roman");
                case text::font_slant::italic:
                    return format_to(ctx.out(), "Italic");
                case text::font_slant::oblique:
                    return format_to(ctx.out(), "Oblique");
            }
            return format_to(ctx.out(), "({})", unsigned(_value));
        }
    };

    template <>
    struct formatter<text::font_spacing> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_spacing _value, FormatContext& ctx)
        {
            switch (_value)
            {
                case text::font_spacing::proportional:
                    return format_to(ctx.out(), "Proportional");
                case text::font_spacing::mono:
                    return format_to(ctx.out(), "Monospace");
            }
            return format_to(ctx.out(), "({})", unsigned(_value));
        }
    };

    template <>
    struct formatter<text::font_description> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_description const& _desc, FormatContext& ctx)
        {
            return format_to(ctx.out(), "(family={} weight={} slant={} spacing={})",
                                        _desc.familyName,
                                        _desc.weight,
                                        _desc.slant,
                                        _desc.spacing);
        }
    };

    template <>
    struct formatter<text::font_metrics> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_metrics const& _metrics, FormatContext& ctx)
        {
            return format_to(ctx.out(), "({}, {}, {}, {}, {}, {})",
                                        _metrics.line_height,
                                        _metrics.advance,
                                        _metrics.ascender,
                                        _metrics.descender,
                                        _metrics.underline_position,
                                        _metrics.underline_thickness);
        }
    };

    template <>
    struct formatter<text::font_size> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_size _size, FormatContext& ctx)
        {
            return format_to(ctx.out(), "{}pt", _size.pt);
        }
    };

    template <>
    struct formatter<text::font_key> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::font_key _key, FormatContext& ctx)
        {
            return format_to(ctx.out(), "{}", _key.value);
        }
    };

    template <>
    struct formatter<text::glyph_index> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::glyph_index _value, FormatContext& ctx)
        {
            return format_to(ctx.out(), "{}", _value.value);
        }
    };

    template <>
    struct formatter<text::glyph_key> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::glyph_key const& _key, FormatContext& ctx)
        {
            return format_to(ctx.out(), "({}, {}, {})", _key.font, _key.size, _key.index);
        }
    };

    template <>
    struct formatter<text::render_mode> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(text::render_mode _value, FormatContext& ctx)
        {
            switch (_value)
            {
                case text::render_mode::bitmap: return format_to(ctx.out(), "Bitmap");
                case text::render_mode::gray: return format_to(ctx.out(), "Gray");
                case text::render_mode::light: return format_to(ctx.out(), "Light");
                case text::render_mode::lcd: return format_to(ctx.out(), "LCD");
                case text::render_mode::color: return format_to(ctx.out(), "Color");
            }
            return format_to(ctx.out(), "({})", unsigned(_value));
        }
    };
} // }}}
