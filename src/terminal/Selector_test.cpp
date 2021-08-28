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
#include <terminal/Screen.h>
#include <terminal/Selector.h>
#include <catch2/catch_all.hpp>

using crispy::Size;
using namespace std;
using namespace std::placeholders;
using namespace terminal;

// Different cases to test
// - single cell
// - inside single line
// - multiple lines
// - multiple lines fully in history
// - multiple lines from history into main buffer
// all of the above with and without scrollback != 0.

namespace
{
    [[maybe_unused]]
    void logScreenTextAlways(Screen const& screen, string const& headline = "")
    {
        fmt::print("{}: ZI={} cursor={} HM={}..{}\n",
                headline.empty() ? "screen dump"s : headline,
                screen.grid().zero_index(),
                screen.realCursorPosition(),
                screen.margin().horizontal.from,
                screen.margin().horizontal.to
        );
        fmt::print("{}\n", dumpGrid(screen.grid()));
    }

    struct TextSelection {
        string text;

        void operator()(Coordinate const& _pos, Cell const& _cell)
        {
            text += _pos.column < lastColumn_ ? "\n" : "";
            text += _cell.toUtf8();
            lastColumn_ = _pos.column;
        }

      private:
        ColumnOffset lastColumn_ = ColumnOffset(0);
    };
}

TEST_CASE("Selector.Linear", "[selector]")
{
    auto screenEvents = ScreenEvents{};
    auto screen = Screen{PageSize{LineCount(3), ColumnCount(11)}, screenEvents,
                         false, false, LineCount(5)};
    screen.write(
        //       0123456789A
        /* 0 */ "12345,67890"s +
        /* 1 */ "ab,cdefg,hi"s +
        /* 2 */ "12345,67890"s
    );

    logScreenTextAlways(screen, "init");
    REQUIRE(screen.grid().lineText(LineOffset(0)) == "12345,67890");
    REQUIRE(screen.grid().lineText(LineOffset(1)) == "ab,cdefg,hi");
    REQUIRE(screen.grid().lineText(LineOffset(2)) == "12345,67890");

#if 0
    SECTION("single-cell") { // "b"
        auto const pos = Coordinate{LineOffset(1), ColumnOffset(1)};
        auto selector = Selector{Selector::Mode::Linear, U",", screen, pos};
        selector.extend(pos);
        selector.stop();

        vector<Selector::Range> const selection = selector.selection();
        REQUIRE(selection.size() == 1);
        Selector::Range const& r1 = selection[0];
        CHECK(r1.line == *pos.line);
        CHECK(r1.fromColumn == *pos.column);
        CHECK(r1.toColumn == *pos.column);
        CHECK(r1.length() == 1);

        auto selectedText = TextSelection{};
        selector.render(selectedText);
        CHECK(selectedText.text == "b");
    }

    SECTION("forward single-line") { // "b,c"
        auto selector = Selector{Selector::Mode::Linear, U",", screen, Coordinate{LineOffset(1), ColumnOffset(1)}};
        selector.extend(Coordinate{LineOffset(1), ColumnOffset(3)});
        selector.stop();

        vector<Selector::Range> const selection = selector.selection();
        REQUIRE(selection.size() == 1);
        Selector::Range const& r1 = selection[0];
        CHECK(r1.line == 1);
        CHECK(r1.fromColumn == 1);
        CHECK(r1.toColumn == 3);
        CHECK(r1.length() == 3);

        auto selectedText = TextSelection{};
        selector.render(selectedText);
        CHECK(selectedText.text == "b,c");
    }

    SECTION("forward multi-line") { // "b,cdefg,hi\n1234"
        auto selector = Selector{Selector::Mode::Linear, U",", screen, Coordinate{LineOffset(1), ColumnOffset(1)}};
        selector.extend(Coordinate{LineOffset(2), ColumnOffset(3)});
        selector.stop();

        vector<Selector::Range> const selection = selector.selection();
        REQUIRE(selection.size() == 2);

        Selector::Range const& r1 = selection[0];
        CHECK(r1.line == 1);
        CHECK(r1.fromColumn == 1);
        CHECK(r1.toColumn == 10);
        CHECK(r1.length() == 10);

        Selector::Range const& r2 = selection[1];
        CHECK(r2.line == 2);
        CHECK(r2.fromColumn == 0);
        CHECK(r2.toColumn == 3);
        CHECK(r2.length() == 4);

        auto selectedText = TextSelection{};
        selector.render(selectedText);
        CHECK(selectedText.text == "b,cdefg,hi\n1234");
    }

    SECTION("multiple lines fully in history") {
        screen.write("foo\r\nbar\r\n"); // move first two lines into history.
        /*
         * |  0123456789A
        -2 | "12345,67890"
        -1 | "ab,cdefg,hi"       [fg,hi]
         0 | "12345,67890"       [123]
         1 | "foo"
         2 | "bar"
        */

        logScreenTextAlways(screen);
        auto selector = Selector{Selector::Mode::Linear, U",", screen, Coordinate{LineOffset(-2), ColumnOffset(6)}};
        selector.extend(Coordinate{LineOffset(-1), ColumnOffset(2)});
        selector.stop();

        vector<Selector::Range> const selection = selector.selection();
        REQUIRE(selection.size() == 2);

        Selector::Range const& r1 = selection[0];
        CHECK(r1.line == -2);
        CHECK(r1.fromColumn == 6);
        CHECK(r1.toColumn == 10);
        CHECK(r1.length() == 5);

        Selector::Range const& r2 = selection[1];
        CHECK(r2.line == -1);
        CHECK(r2.fromColumn == 0);
        CHECK(r2.toColumn == 2);
        CHECK(r2.length() == 3);

        auto selectedText = TextSelection{};
        selector.render(selectedText);
        CHECK(selectedText.text == "fg,hi\n123");
    }
#endif

    SECTION("multiple lines from history into main buffer") {
        logScreenTextAlways(screen, "just before next test-write");
        screen.write("foo\r\nbar\r\n"); // move first two lines into history.
        logScreenTextAlways(screen, "just after next test-write");
        /*
        -3 | "12345,67890"
        -2 | "ab,cdefg,hi"         (--
        -1 | "12345,67890" -----------
         0 | "foo"         --)
         1 | "bar"
         2 | ""
        */

        auto selector = Selector{Selector::Mode::Linear, U",", screen,
                                 Coordinate{LineOffset(-2), ColumnOffset(8)}};
        selector.extend(Coordinate{LineOffset(0), ColumnOffset(1)});
        selector.stop();

        vector<Selector::Range> const selection = selector.selection();
        REQUIRE(selection.size() == 3);

        Selector::Range const& r1 = selection[0];
        CHECK(r1.line == -2);
        CHECK(r1.fromColumn == 8);
        CHECK(r1.toColumn == 10);
        CHECK(r1.length() == 3);

        Selector::Range const& r2 = selection[1];
        CHECK(r2.line == -1);
        CHECK(r2.fromColumn == 0);
        CHECK(r2.toColumn == 10);
        CHECK(r2.length() == 11);

        Selector::Range const& r3 = selection[2];
        CHECK(r3.line == 0);
        CHECK(r3.fromColumn == 0);
        CHECK(r3.toColumn == 1);
        CHECK(r3.length() == 2);

        auto selectedText = TextSelection{};
        selector.render(selectedText);
        CHECK(selectedText.text == ",hi\n12345,67890\nfo");
    }
}

TEST_CASE("Selector.LinearWordWise", "[selector]")
{
    // TODO
}

TEST_CASE("Selector.FullLine", "[selector]")
{
    // TODO
}

TEST_CASE("Selector.Rectangular", "[selector]")
{
    // TODO
}
