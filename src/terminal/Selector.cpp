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
#include <terminal/Selector.h>
#include <terminal/Screen.h>
#include <terminal/Terminal.h>
#include <crispy/times.h>
#include <cassert>

using namespace std;

namespace terminal {

Selector::Selector(Mode _mode,
				   GetCellAt _getCellAt,
                   GetWrappedFlag _wrappedFlag,
				   std::u32string const& _wordDelimiters,
				   LineCount _totalRowCount,
				   ColumnCount _columnCount,
				   Coordinate _from) :
	mode_{_mode},
	getCellAt_{move(_getCellAt)},
    wrapped_{move(_wrappedFlag)},
	wordDelimiters_{_wordDelimiters},
	totalRowCount_{_totalRowCount},
    columnCount_{_columnCount},
	start_{_from},
	from_{_from},
	to_{_from}
{
	if (_mode == Mode::FullLine)
	{
		extend(from_.line, ColumnOffset(0));
		swapDirection();
		extend(from_.line, columnCount_.as<ColumnOffset>());

        // backward
        while (from_.line > LineOffset(0) && wrapped_(from_.line))
            from_.line--;

        // forward
        while (*to_.line < *_totalRowCount && wrapped_(to_.line + 1))
            to_.line++;
	}
	else if (isWordWiseSelection())
	{
        // TODO: expand logical line to complete word, if on line boundary
		state_ = State::InProgress;
		extendSelectionBackward();
		swapDirection();
		extendSelectionForward();
	}
}

Selector::Selector(Mode _mode,
                   std::u32string const& _wordDelimiters,
                   Screen const& _screen,
                   Coordinate _from) :
    Selector{
        _mode,
        [screen = std::ref(_screen)](LineOffset _line, ColumnOffset _column) -> Cell const* {
            auto const& buffer = screen.get();
            return &buffer.at(_line, _column);
        },
        [screen = std::ref(_screen)](LineOffset _line) -> bool {
            return screen.get().isLineWrapped(_line);
        },
        _wordDelimiters,
        _screen.pageSize().lines + _screen.historyLineCount(),
        _screen.pageSize().columns,
        _from
    }
{
}

Coordinate Selector::stretchedColumn(Coordinate _coord) const noexcept
{
    Coordinate stretched = _coord;
    if (Cell const* cell = at(_coord); cell && cell->width() > 1)
    {
        // wide character
        stretched.column += ColumnOffset::cast_from(cell->width() - 1);
        return stretched;
    }

    while (*stretched.column < *columnCount_)
    {
        if (Cell const* cell = at(stretched); cell)
        {
            if (cell->empty())
                stretched.column++;
            else
            {
                if (cell->width() > 1)
                    stretched.column += ColumnOffset::cast_from(cell->width() - 1);
                break;
            }
        }
        else
            break;
    }

    return stretched;
}

bool Selector::extend(LineOffset _line, ColumnOffset _column)
{
    assert(state_ != State::Complete && "In order extend a selection, the selector must be active (started).");

    auto column = clamp(_column, ColumnOffset(0), columnCount_.as<ColumnOffset>());
    auto const coord = Coordinate{_line, column};

    state_ = State::InProgress;

    switch (mode_)
    {
        case Mode::FullLine: // TODO(pr) something's broken with line-wise selection (tripple click), just try it, you'll get assert()
            if (coord > start_)
            {
                to_ = coord;
                while (*to_.line + 1 < *totalRowCount_ && wrapped_(to_.line + 1))
                    to_.line++;
            }
            else if (coord < start_)
            {
                from_ = coord;
                while (*from_.line > 0 && wrapped_(from_.line))
                    from_.line--;
            }
            break;
        case Mode::Linear:
            to_ = stretchedColumn(coord);
            break;
        case Mode::LinearWordWise:
            // TODO: handle logical line wraps
        case Mode::Rectangular:
            if (coord > start_)
            {
                to_ = coord;
                extendSelectionForward();
            }
            else
            {
                to_ = coord;
                extendSelectionBackward(); //TODO adapt
                swapDirection();
                to_ = start_;
                extendSelectionForward();
            }
            break;
    }

    // TODO: indicates whether or not a scroll action must take place.
    return false;
}

void Selector::extendSelectionBackward()
{
    auto const isWordDelimiterAt = [this](Coordinate const& _coord) -> bool {
        Cell const* cell = at(_coord);
        return !cell || cell->empty() || wordDelimiters_.find(cell->codepoint(0)) != wordDelimiters_.npos;
    };

    auto last = to_;
    auto current = last;
    for (;;) {
        auto const wrapIntoPreviousLine = *current.column == 1 && *current.line > 0 && wrapped_(current.line);
        if (*current.column > 1)
            current.column--;
        else if (*current.line > 0 || wrapIntoPreviousLine)
        {
            current.line--;
            current.column = columnCount_.as<ColumnOffset>();
        }
        else
            break;

        if (isWordDelimiterAt(current))
            break;
        last = current;
    }

    if (to_ < from_)
    {
        swapDirection();
		to_ = last;
    }
    else
        to_ = last;
}

void Selector::extendSelectionForward()
{
    auto const isWordDelimiterAt = [this](Coordinate _coord) -> bool {
        Cell const* cell = at(_coord);
        return !cell || cell->empty() || wordDelimiters_.find(cell->codepoint(0)) != wordDelimiters_.npos;
    };

    auto last = to_;
    auto current = last;
    for (;;) {
        if (*current.column == *columnCount_ && *current.line + 1 < *totalRowCount_ && wrapped_(current.line + 1))
        {
            current.line++;
            current.column = ColumnOffset(0);
            current = stretchedColumn({current.line, current.column + 1});
        }

        if (*current.column < *columnCount_)
        {
            current = stretchedColumn({current.line, current.column + 1});
        }
        else if (*current.line < *totalRowCount_)
        {
            current.line++;
            current.column = ColumnOffset(0);
        }
        else
            break;

        if (isWordDelimiterAt(current))
            break;
        last = current;
    }

    to_ = stretchedColumn(last);
}

void Selector::stop()
{
    if (state_ == State::InProgress)
        state_ = State::Complete;
}

tuple<vector<Selector::Range>, Coordinate const, Coordinate const> prepare(Selector const& _selector)
{
    vector<Selector::Range> result;

    auto const [from, to] = [&]() {
        if (_selector.to() < _selector.from())
            return pair{_selector.to(), _selector.from()};
        else
            return pair{_selector.from(), _selector.to()};
    }();

    auto const numLines = to.line - from.line + 1;
    result.resize(numLines.as<size_t>());

    return {move(result), from, to};
}

vector<Selector::Range> Selector::selection() const
{
	switch (mode_)
	{
		case Mode::FullLine:
			return lines();
		case Mode::Linear:
		case Mode::LinearWordWise:
			return linear();
		case Mode::Rectangular:
			return rectangular();
	}
	return {};
}

vector<Selector::Range> Selector::linear() const
{
    auto [result, from, to] = prepare(*this);

    switch (result.size())
    {
        case 1:
            result[0] = Range{*from.line, *from.column, *to.column};
            break;
        case 2:
            // Render first line partial from selected column to end.
            result[0] = Range{*from.line, *from.column, *columnCount_ - 1};
            // Render last (second) line partial from beginning to last selected column.
            result[1] = Range{*to.line, 0, *to.column};
            break;
        default:
            // Render first line partial from selected column to end.
            result[0] = Range{*from.line, *from.column, *columnCount_ - 1};

            // Render inner full.
            for (size_t n = 1; n < result.size(); ++n)
                result[n] = Range{*from.line + static_cast<int>(n), 0, *columnCount_ - 1};

            // Render last (second) line partial from beginning to last selected column.
            result[result.size() - 1] = Range{*to.line, 0, *to.column};
            break;
    }

    return result;
}

vector<Selector::Range> Selector::lines() const
{
    auto [result, from, to] = prepare(*this);

    for (size_t line = 0; line < result.size(); ++line)
    {
        result[line] = Range{
            *from.line + static_cast<int>(line),
            1,
            *columnCount_
        };
    }

    return result;
}

vector<Selector::Range> Selector::rectangular() const
{
    auto [result, from, to] = prepare(*this);

    for (size_t line = 0; line < result.size(); ++line)
    {
        result[line] = Range{
            *from.line + static_cast<int>(line),
            *from.column,
            *to.column
        };
    }

    return result;
}

} // namespace terminal
