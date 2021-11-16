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

#include <terminal/InputGenerator.h>
//#include <terminal/Screen.h>

#include <crispy/size.h>
#include <crispy/utils.h>

#include <fmt/format.h>

#include <functional>
#include <vector>
#include <utility>

namespace terminal {

class Screen;
class Cell;

/**
 * Selector API.
 *
 * A Selector can select a range of text. The range can be linear with partial start/end lines, or full lines,
 * or a block based selector, that is capable of selecting all lines partially.
 *
 * The Selector operates on the Screen by accumulating a scrolling offset, that determines
 * the view port of that Screen.
 *
 * When the screen is being modified while selecting text, the selection regions must be preserved,
 * that is, when the selection start is inside the screen, then new lines are added, which causes the screen
 * to move the screen contents up, then also the selection's begin (and extend) is being moved up.
 *
 * This is achieved by using absolute coordinates from the top history line.
 *
 * How Selection usually works
 * ===========================
 *
 * First mouse press ->
 * Second mouse press AND on same coordinate as first mouse press -> selects word
 * Third mouse press AND on same coordinate as prior mouse presses -> reselects line
 * Mouse moves -> resets last recorded mouse press coordinate
 */
class Selector {
  public:
    enum class State {
        /// Inactive, but waiting for the selection to be started (by moving the cursor).
        Waiting,
        /// Active, with selection in progress.
        InProgress,
        /// Inactive, with selection available.
        Complete,
    };

    struct Range {
        int line;
        int fromColumn;
        int toColumn;

        constexpr int length() const noexcept { return toColumn - fromColumn + 1; }
    };

    enum class Mode { Linear, LinearWordWise, FullLine, Rectangular };
	using GetCellAt = std::function<Cell const*(LineOffset, ColumnOffset)>;
    using GetWrappedFlag = std::function<bool(LineOffset)>;

    Selector(Mode _mode,
			 GetCellAt _at,
             GetWrappedFlag _wrappedFlag,
			 std::u32string const& _wordDelimiters,
			 LineCount _totalRowCount,
             ColumnCount _columnCount,
			 Coordinate _from);

	/// Convenience constructor when access to Screen is available.
    Selector(Mode _mode,
			 std::u32string const& _wordDelimiters,
			 Screen const& _screen,
			 Coordinate _from);

    /// Tests whether the a selection is currently in progress.
    constexpr State state() const noexcept { return state_; }

    /// @todo Should be able to handle negative (or 0) and overflow coordinates,
    ///       which should potentially adjust the screen's view (aka. modifying scrolling offset).
    ///
    /// @retval true TerminalView requires scrolling offset adjustments.
    /// @retval false TerminalView's scrolling offset does not need adjustments.
    bool extend(Coordinate _to)
    {
        return extend(_to.line, _to.column);
    }

    bool extend(LineOffset _line, ColumnOffset _column);

    /// Marks the selection as completed.
    void stop();

    constexpr Coordinate const& from() const noexcept { return from_; }
    constexpr Coordinate const& to() const noexcept { return to_; }

    /// @returns boolean indicating whether or not given absolute coordinate is within the range of the selection.
    constexpr bool contains(Coordinate _coord) const noexcept
    {
        switch (mode_)
        {
            case Mode::FullLine:
                return crispy::ascending(from_.line, _coord.line, to_.line)
                    || crispy::ascending(to_.line, _coord.line, from_.line);
            case Mode::Linear:
            case Mode::LinearWordWise:
                return crispy::ascending(from_, _coord, to_)
                    || crispy::ascending(to_, _coord, from_);
            case Mode::Rectangular:
                return crispy::ascending(from_.line, _coord.line, to_.line)
                    && crispy::ascending(from_.column, _coord.column, to_.column);
        }
        return false;
    }

    constexpr Mode mode() const noexcept { return mode_; }

    /// Tests whether selection is upwards.
    constexpr bool negativeSelection() const noexcept { return to_ < from_; }
    constexpr bool singleLineSelection() const noexcept { return from_.line == to_.line; }

    constexpr void swapDirection() noexcept
    {
        swap(from_, to_);
    }

    /// Eventually stretches the coordinate a few cells to the right if the cell at given coordinate
    /// contains a wide character - or if the cell is empty, until the end of emptyness.
    Coordinate stretchedColumn(Coordinate _pos) const noexcept;

	/// Retrieves a vector of ranges (with one range per line) of selected cells.
	std::vector<Range> selection() const;

	/// Constructs a vector of ranges for a linear selection strategy.
	std::vector<Range> linear() const;

	/// Constructs a vector of ranges for a full-line selection strategy.
	std::vector<Range> lines() const;

	/// Constructs a vector of ranges for a rectangular selection strategy.
	std::vector<Range> rectangular() const;

    /// Renders the current selection into @p _render.
    template <typename Renderer>
    void render(Renderer&& _render) const
    {
        for (auto const& range: selection())
        {
            for (auto const col: crispy::times(range.fromColumn, range.length()))
            {
                auto line = LineOffset::cast_from(range.line);
                auto column = ColumnOffset::cast_from(col);
                Cell const* cell = at(line, column);
                if (cell != nullptr)
                    _render(Coordinate{line, column}, *cell);
            }
        }
    }

  private:
	bool isWordWiseSelection() const noexcept
	{
		switch (mode_)
		{
			case Mode::LinearWordWise:
				return true;
			default:
				return false;
		}
	}

	Cell const* at(Coordinate _coord) const noexcept
    {
        return getCellAt_(_coord.line, _coord.column);
    }

	Cell const* at(LineOffset _line, ColumnOffset _column) const noexcept
    {
        return getCellAt_(_line, _column);
    }

	void extendSelectionBackward();
	void extendSelectionForward();

    // private fields
    //
    State state_{State::Waiting};
	Mode mode_;
	GetCellAt getCellAt_;
    GetWrappedFlag wrapped_;
	std::u32string wordDelimiters_;
	LineCount totalRowCount_;
    ColumnCount columnCount_;
    Coordinate start_{};
    Coordinate from_{};
    Coordinate to_{};
};

} // namespace terminal

namespace fmt { // {{{
    template <>
    struct formatter<terminal::Selector::State> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }
        using State = terminal::Selector::State;
        template <typename FormatContext>
        auto format(State _state, FormatContext& ctx)
        {
            switch (_state)
            {
                case State::Waiting:
                    return format_to(ctx.out(), "Waiting");
                case State::InProgress:
                    return format_to(ctx.out(), "InProgress");
                case State::Complete:
                    return format_to(ctx.out(), "Complete");
            }
            return format_to(ctx.out(), "{}", static_cast<unsigned>(_state));
        }
    };

    template <>
    struct formatter<terminal::Selector> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const terminal::Selector& _selector, FormatContext& ctx)
        {
            return format_to(ctx.out(),
                             "({} .. {}; state: {})",
                             _selector.from(),
                             _selector.to(),
                             _selector.state());
        }
    };
} // }}}
