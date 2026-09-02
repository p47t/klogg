/*
 * Copyright (C) 2026 Anton Filimonov and other contributors
 *
 * This file is part of klogg.
 *
 * klogg is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * klogg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with klogg.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include "wrappedstring.h"

SCENARIO( "Wrapping a line", "[wrappedstring]" )
{
    // Columns:      0123456789012345678901234
    const QString line{ "aaa bbb ccc ddd eee fff" };

    GIVEN( "A line shorter than the view" )
    {
        WrappedString wrapped{ line, 100_length };

        THEN( "It stays on a single line" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 1 );
            REQUIRE( wrapped.wrappedLine( 0 ) == line );
        }
    }

    GIVEN( "A line longer than the view" )
    {
        WrappedString wrapped{ line, 12_length };

        THEN( "It wraps on word boundaries" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 2 );
            REQUIRE( wrapped.wrappedLine( 0 ) == "aaa bbb ccc " );
            REQUIRE( wrapped.wrappedLine( 1 ) == "ddd eee fff" );
        }
    }

    GIVEN( "A narrower first line, to leave room for an annotation" )
    {
        WrappedString wrapped{ line, 12_length, 8_length };

        THEN( "Only the first line uses the narrower width" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 3 );
            REQUIRE( wrapped.wrappedLine( 0 ) == "aaa bbb " );
            // The remaining lines get the full width again
            REQUIRE( wrapped.wrappedLine( 1 ) == "ccc ddd eee " );
            REQUIRE( wrapped.wrappedLine( 2 ) == "fff" );
        }
    }

    GIVEN( "A narrower first line on a line that would otherwise fit" )
    {
        WrappedString wrapped{ line, 100_length, 12_length };

        THEN( "The line is wrapped early anyway" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 2 );
            REQUIRE( wrapped.wrappedLine( 0 ) == "aaa bbb ccc " );
            REQUIRE( wrapped.wrappedLine( 1 ) == "ddd eee fff" );
        }
    }

    GIVEN( "A first line width of zero" )
    {
        WrappedString wrapped{ line, 12_length, 0_length };

        THEN( "It is ignored and the normal width is used throughout" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 2 );
            REQUIRE( wrapped.wrappedLine( 0 ) == "aaa bbb ccc " );
        }
    }

    GIVEN( "A word longer than the view" )
    {
        WrappedString wrapped{ QString{ "aaaaaaaaaa" }, 4_length, 2_length };

        THEN( "It is cut at the width of each line" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 3 );
            REQUIRE( wrapped.wrappedLine( 0 ) == "aa" );
            REQUIRE( wrapped.wrappedLine( 1 ) == "aaaa" );
            REQUIRE( wrapped.wrappedLine( 2 ) == "aaaa" );
        }
    }

    GIVEN( "An empty line" )
    {
        WrappedString wrapped{ QString{}, 12_length, 4_length };

        THEN( "It has a single empty wrapped line" )
        {
            REQUIRE( wrapped.wrappedLinesCount() == 1 );
            REQUIRE( wrapped.isEmpty() );
        }
    }
}
