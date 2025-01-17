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

#include <terminal/CellFlags.h>
#include <terminal/CellUtil.h>
#include <terminal/ColorPalette.h>
#include <terminal/GraphicsAttributes.h>
#include <terminal/Hyperlink.h>
#include <terminal/Image.h>
#include <terminal/primitives.h>

#include <crispy/Owned.h>
#include <crispy/defines.h>
#include <crispy/times.h>

#include <unicode/capi.h>
#include <unicode/convert.h>
#include <unicode/width.h>

#include <memory>
#include <string>

#define LIBTERMINAL_GRAPHEME_CLUSTERS 1

namespace terminal
{

/// Rarely needed extra cell data.
///
/// In this struct we collect all the relevant cell data that is not frequently used,
/// and thus, would only waste unnecessary memory in most situations.
///
/// @see CompactCell
struct CellExtra
{
    /// With the main codepoint that is being stored in the CompactCell struct, followed by this
    /// sequence of codepoints, a grapheme cluster is formed that represents the visual
    /// character in this terminal cell.
    ///
    /// Since MOST content in the terminal is US-ASCII, all codepoints except the first one of a grapheme
    /// cluster is stored in CellExtra.
    std::u32string codepoints = {};

    /// Color for underline decoration (such as curly underline).
    Color underlineColor = DefaultColor();

    /// With OSC-8 a hyperlink can be associated with a range of terminal cells.
    HyperlinkId hyperlink = {};

    /// Holds a reference to an image tile to be rendered (above the text, if any).
    std::shared_ptr<ImageFragment> imageFragment = nullptr;

    /// Cell flags.
    CellFlags flags = CellFlags::None;

    /// In terminals, the Unicode's East asian Width property is used to determine the
    /// number of columns, a graphical character is spanning.
    /// Since most graphical characters in a terminal will be US-ASCII, this width property
    /// will be only used when NOT being 1.
    uint8_t width = 1;
};

/// Grid cell with character and graphics rendition information.
///
/// TODO(perf): ensure POD'ness so that we can SIMD-copy it.
/// - Requires moving out CellExtra into Line<T>?
class CRISPY_PACKED CompactCell
{
  public:
    static uint8_t constexpr MaxCodepoints = 7;

    CompactCell() noexcept;
    CompactCell(CompactCell const& v) noexcept;
    CompactCell& operator=(CompactCell const& v) noexcept;
    explicit CompactCell(GraphicsAttributes _attributes, HyperlinkId hyperlink = {}) noexcept;

    CompactCell(CompactCell&&) noexcept = default;
    CompactCell& operator=(CompactCell&&) noexcept = default;
    ~CompactCell() = default;

    void reset() noexcept;
    void reset(GraphicsAttributes const& _attributes) noexcept;
    void reset(GraphicsAttributes const& _attributes, HyperlinkId _hyperlink) noexcept;

    void write(GraphicsAttributes const& _attributes, char32_t _ch, uint8_t _width) noexcept;
    void write(GraphicsAttributes const& _attributes,
               char32_t _ch,
               uint8_t _width,
               HyperlinkId _hyperlink) noexcept;

    void writeTextOnly(char32_t _ch, uint8_t _width) noexcept;

    [[nodiscard]] std::u32string codepoints() const;
    [[nodiscard]] char32_t codepoint(size_t i) const noexcept;
    [[nodiscard]] std::size_t codepointCount() const noexcept;

    [[nodiscard]] constexpr uint8_t width() const noexcept;
    void setWidth(uint8_t _width) noexcept;

    [[nodiscard]] CellFlags flags() const noexcept;

    [[nodiscard]] bool isFlagEnabled(CellFlags testFlags) const noexcept { return flags() & testFlags; }

    void resetFlags() noexcept
    {
        if (extra_)
            extra_->flags = CellFlags::None;
    }

    void resetFlags(CellFlags flags) noexcept { extra().flags = flags; }

    [[nodiscard]] Color underlineColor() const noexcept;
    void setUnderlineColor(Color color) noexcept;
    [[nodiscard]] Color foregroundColor() const noexcept;
    void setForegroundColor(Color color) noexcept;
    [[nodiscard]] Color backgroundColor() const noexcept;
    void setBackgroundColor(Color color) noexcept;

    [[nodiscard]] std::shared_ptr<ImageFragment> imageFragment() const noexcept;
    void setImageFragment(std::shared_ptr<RasterizedImage> rasterizedImage, CellLocation offset);

    void setCharacter(char32_t _codepoint) noexcept;
    [[nodiscard]] int appendCharacter(char32_t _codepoint) noexcept;
    [[nodiscard]] std::string toUtf8() const;

    [[nodiscard]] HyperlinkId hyperlink() const noexcept;
    void setHyperlink(HyperlinkId _hyperlink);

    [[nodiscard]] bool empty() const noexcept;

    void setGraphicsRendition(GraphicsRendition sgr) noexcept;

  private:
    [[nodiscard]] CellExtra& extra() noexcept;

    template <typename... Args>
    void createExtra(Args... args) noexcept;

    // CompactCell data
    char32_t codepoint_ = 0; /// Primary Unicode codepoint to be displayed.
    Color foregroundColor_ = DefaultColor();
    Color backgroundColor_ = DefaultColor();
    crispy::Owned<CellExtra> extra_ = {};
    // TODO(perf) ^^ use CellExtraId = boxed<int24_t> into pre-alloc'ed vector<CellExtra>.
};

// {{{ impl: ctor's
template <typename... Args>
inline void CompactCell::createExtra(Args... args) noexcept
{
    try
    {
        extra_.reset(new CellExtra(std::forward<Args>(args)...));
    }
    catch (std::bad_alloc const&)
    {
        Require(extra_.get() != nullptr);
    }
}

inline CompactCell::CompactCell() noexcept
{
    setWidth(1);
}

inline CompactCell::CompactCell(GraphicsAttributes _attributes, HyperlinkId hyperlink) noexcept:
    foregroundColor_ { _attributes.foregroundColor }, backgroundColor_ { _attributes.backgroundColor }
{
    setWidth(1);
    setHyperlink(hyperlink);

    if (_attributes.underlineColor != DefaultColor() || extra_)
        extra().underlineColor = _attributes.underlineColor;

    if (_attributes.flags != CellFlags::None || extra_)
        extra().flags = _attributes.flags;
}

inline CompactCell::CompactCell(CompactCell const& v) noexcept:
    codepoint_ { v.codepoint_ },
    foregroundColor_ { v.foregroundColor_ },
    backgroundColor_ { v.backgroundColor_ }
{
    if (v.extra_)
        createExtra(*v.extra_);
}

inline CompactCell& CompactCell::operator=(CompactCell const& v) noexcept
{
    codepoint_ = v.codepoint_;
    foregroundColor_ = v.foregroundColor_;
    backgroundColor_ = v.backgroundColor_;
    if (v.extra_)
        createExtra(*v.extra_);
    return *this;
}
// }}}
// {{{ impl: reset
inline void CompactCell::reset() noexcept
{
    codepoint_ = 0;
    foregroundColor_ = DefaultColor();
    backgroundColor_ = DefaultColor();
    extra_.reset();
}

inline void CompactCell::reset(GraphicsAttributes const& _attributes) noexcept
{
    codepoint_ = 0;
    foregroundColor_ = _attributes.foregroundColor;
    backgroundColor_ = _attributes.backgroundColor;
    extra_.reset();
    if (_attributes.flags != CellFlags::None)
        extra().flags = _attributes.flags;
    if (_attributes.underlineColor != DefaultColor())
        extra().underlineColor = _attributes.underlineColor;
}

inline void CompactCell::write(GraphicsAttributes const& _attributes, char32_t _ch, uint8_t _width) noexcept
{
    setWidth(_width);

    codepoint_ = _ch;
    if (extra_)
    {
        extra_->codepoints.clear();
        extra_->imageFragment = {};
    }

    foregroundColor_ = _attributes.foregroundColor;
    backgroundColor_ = _attributes.backgroundColor;

    if (_attributes.flags != CellFlags::None || extra_)
        extra().flags = _attributes.flags;

    if (_attributes.underlineColor != DefaultColor())
        extra().underlineColor = _attributes.underlineColor;
}

inline void CompactCell::write(GraphicsAttributes const& _attributes,
                               char32_t _ch,
                               uint8_t _width,
                               HyperlinkId _hyperlink) noexcept
{
    writeTextOnly(_ch, _width);
    if (extra_)
    {
        // Writing text into a cell destroys the image fragment (as least for Sixels).
        extra_->imageFragment = {};
    }

    foregroundColor_ = _attributes.foregroundColor;
    backgroundColor_ = _attributes.backgroundColor;

    if (_attributes.flags != CellFlags::None || extra_ || _attributes.underlineColor != DefaultColor()
        || !!_hyperlink)
    {
        CellExtra& ext = extra();
        ext.underlineColor = _attributes.underlineColor;
        ext.hyperlink = _hyperlink;
        ext.flags = _attributes.flags;
    }
}

inline void CompactCell::writeTextOnly(char32_t _ch, uint8_t _width) noexcept
{
    setWidth(_width);
    codepoint_ = _ch;
    if (extra_)
        extra_->codepoints.clear();
}

inline void CompactCell::reset(GraphicsAttributes const& _attributes, HyperlinkId _hyperlink) noexcept
{
    codepoint_ = 0;
    foregroundColor_ = _attributes.foregroundColor;
    backgroundColor_ = _attributes.backgroundColor;

    extra_.reset();
    if (_attributes.underlineColor != DefaultColor())
        extra().underlineColor = _attributes.underlineColor;
    if (_attributes.flags != CellFlags::None)
        extra().flags = _attributes.flags;
    if (_hyperlink != HyperlinkId())
        extra().hyperlink = _hyperlink;
}
// }}}
// {{{ impl: character
inline constexpr uint8_t CompactCell::width() const noexcept
{
    return !extra_ ? 1 : extra_->width;
    // return static_cast<int>((codepoint_ >> 21) & 0x03); //return width_;
}

inline void CompactCell::setWidth(uint8_t _width) noexcept
{
    assert(_width < MaxCodepoints);
    // codepoint_ = codepoint_ | ((_width << 21) & 0x03);
    if (_width > 1 || extra_)
        extra().width = _width;
    // TODO(perf) use u32_unused_bit_mask()
}

inline void CompactCell::setCharacter(char32_t _codepoint) noexcept
{
    codepoint_ = _codepoint;
    if (extra_)
    {
        extra_->codepoints.clear();
        extra_->imageFragment = {};
    }
    if (_codepoint)
        setWidth(static_cast<uint8_t>(std::max(unicode::width(_codepoint), 1)));
    else
        setWidth(1);
}

inline int CompactCell::appendCharacter(char32_t _codepoint) noexcept
{
    assert(_codepoint != 0);

    CellExtra& ext = extra();
    if (ext.codepoints.size() < MaxCodepoints - 1)
    {
        ext.codepoints.push_back(_codepoint);
        if (auto const diff = CellUtil::computeWidthChange(*this, _codepoint))
        {
            setWidth(static_cast<uint8_t>(static_cast<int>(width()) + diff));
            return diff;
        }
    }
    return 0;
}

inline std::size_t CompactCell::codepointCount() const noexcept
{
    if (codepoint_)
    {
        if (!extra_)
            return 1;

        return 1 + extra_->codepoints.size();
    }
    return 0;
}

inline char32_t CompactCell::codepoint(size_t i) const noexcept
{
    if (i == 0)
        return codepoint_;

    if (!extra_)
        return 0;

#if !defined(NDEBUG)
    return extra_->codepoints.at(i - 1);
#else
    return extra_->codepoints[i - 1];
#endif
}
// }}}
// {{{ attrs
inline CellExtra& CompactCell::extra() noexcept
{
    if (extra_)
        return *extra_;
    createExtra();
    return *extra_;
}

inline CellFlags CompactCell::flags() const noexcept
{
    if (!extra_)
        return CellFlags::None;
    else
        return const_cast<CompactCell*>(this)->extra_->flags;
}

inline Color CompactCell::foregroundColor() const noexcept
{
    return foregroundColor_;
}

inline void CompactCell::setForegroundColor(Color color) noexcept
{
    foregroundColor_ = color;
}

inline Color CompactCell::backgroundColor() const noexcept
{
    return backgroundColor_;
}

inline void CompactCell::setBackgroundColor(Color color) noexcept
{
    backgroundColor_ = color;
}

inline Color CompactCell::underlineColor() const noexcept
{
    if (!extra_)
        return DefaultColor();
    else
        return extra_->underlineColor;
}

inline void CompactCell::setUnderlineColor(Color color) noexcept
{
    if (extra_)
        extra_->underlineColor = color;
    else if (color != DefaultColor())
        extra().underlineColor = color;
}

inline std::shared_ptr<ImageFragment> CompactCell::imageFragment() const noexcept
{
    if (extra_)
        return extra_->imageFragment;
    else
        return {};
}

inline void CompactCell::setImageFragment(std::shared_ptr<RasterizedImage> rasterizedImage,
                                          CellLocation offset)
{
    CellExtra& ext = extra();
    ext.imageFragment = std::make_shared<ImageFragment>(rasterizedImage, offset);
}

inline HyperlinkId CompactCell::hyperlink() const noexcept
{
    if (extra_)
        return extra_->hyperlink;
    else
        return HyperlinkId {};
}

inline void CompactCell::setHyperlink(HyperlinkId _hyperlink)
{
    if (!!_hyperlink)
        extra().hyperlink = _hyperlink;
    else if (extra_)
        extra_->hyperlink = {};
}

inline bool CompactCell::empty() const noexcept
{
    return CellUtil::empty(*this);
}

inline void CompactCell::setGraphicsRendition(GraphicsRendition sgr) noexcept
{
    CellUtil::applyGraphicsRendition(sgr, *this);
}

// }}}
// {{{ free function implementations
inline bool beginsWith(std::u32string_view text, CompactCell const& cell) noexcept
{
    assert(text.size() != 0);

    if (cell.codepointCount() == 0)
        return false;

    if (text.size() < cell.codepointCount())
        return false;

    for (size_t i = 0; i < cell.codepointCount(); ++i)
        if (cell.codepoint(i) != text[i])
            return false;

    return true;
}
// }}}

} // namespace terminal

namespace fmt // {{{
{
template <>
struct formatter<terminal::CompactCell>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(terminal::CompactCell const& cell, FormatContext& ctx)
    {
        std::string codepoints;
        for (auto const i: crispy::times(cell.codepointCount()))
        {
            if (i)
                codepoints += ", ";
            codepoints += fmt::format("{:02X}", static_cast<unsigned>(cell.codepoint(i)));
        }
        return fmt::format_to(ctx.out(), "(chars={}, width={})", codepoints, cell.width());
    }
};
} // namespace fmt
