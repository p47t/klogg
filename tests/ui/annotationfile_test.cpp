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

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "test_utils.h"

#include "annotationfile.h"
#include "logdata.h"

namespace {

constexpr int NbLines = 400;

// A log whose lines are all distinct, so that an anchor identifies exactly one
QString lineText( int index )
{
    return QStringLiteral( "GET /api/thing/%1 took %2ms" ).arg( index ).arg( index * 3 );
}

struct LogFixture {
    explicit LogFixture( int firstLine = 0 )
    {
        REQUIRE( dir.isValid() );
        logPath = dir.filePath( QStringLiteral( "app.log" ) );
        write( firstLine );

        SafeQSignalSpy loadEndSpy( &logData, SIGNAL( loadingFinished( LoadingStatus ) ) );
        logData.attachFile( logPath );
        REQUIRE( loadEndSpy.safeWait( 10000 ) );
    }

    // Rewrite the log starting from a different line, which shifts every
    // subsequent line up or down
    void write( int firstLine ) const
    {
        QFile file{ logPath };
        REQUIRE( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
        for ( int i = firstLine; i < firstLine + NbLines; ++i ) {
            file.write( lineText( i ).toUtf8() );
            file.write( "\n", 1 );
        }
        file.close();
    }

    QString sidecarPath() const
    {
        return AnnotationFile::pathForLogFile( logPath );
    }

    void writeSidecar( const QString& contents ) const
    {
        QFile file{ sidecarPath() };
        REQUIRE( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
        file.write( contents.toUtf8() );
        file.close();
    }

    QTemporaryDir dir;
    QString logPath;
    LogData logData;
};

QString entry( int line, const QString& sha1, const QString& text )
{
    if ( sha1.isEmpty() ) {
        return QStringLiteral( R"({"line":%1,"text":"%2"})" ).arg( line ).arg( text );
    }
    return QStringLiteral( R"({"line":%1,"sha1":"%2","text":"%3"})" )
        .arg( line )
        .arg( sha1 )
        .arg( text );
}

QString sidecar( const QStringList& entries, int version = AnnotationFile::FormatVersion )
{
    return QStringLiteral( R"({"version":%1,"annotations":[%2]})" )
        .arg( version )
        .arg( entries.join( ',' ) );
}

} // namespace

SCENARIO( "reading annotations from a sidecar file", "[logdata]" )
{
    GIVEN( "a log file with no sidecar" )
    {
        LogFixture log;

        THEN( "the sidecar is named after the log file" )
        {
            REQUIRE( log.sidecarPath() == log.logPath + ".klogg-annotations.json" );
        }

        THEN( "loading reports nothing was read, without an error" )
        {
            const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );
            REQUIRE_FALSE( result.read );
            REQUIRE( result.error.isEmpty() );
            REQUIRE( result.annotations.empty() );
        }
    }

    GIVEN( "a sidecar anchored to lines that have not moved" )
    {
        LogFixture log;

        // The file speaks 1-based lines, the result is keyed 0-based
        log.writeSidecar( sidecar( {
            entry( 3, AnnotationFile::lineDigest( lineText( 2 ) ), "slow call" ),
            entry( 10, AnnotationFile::lineDigest( lineText( 9 ) ), "retry storm starts here" ),
        } ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "both comments land on the lines they name" )
        {
            REQUIRE( result.read );
            REQUIRE( result.error.isEmpty() );
            REQUIRE( result.annotations.size() == 2 );
            REQUIRE( result.annotations.at( 2_lnum ) == "slow call" );
            REQUIRE( result.annotations.at( 9_lnum ) == "retry storm starts here" );
            REQUIRE( result.relocated == 0 );
            REQUIRE( result.dropped == 0 );
        }
    }

    GIVEN( "a sidecar whose anchored lines have shifted in the log" )
    {
        LogFixture log;

        // Anchor the text of line 100 but claim it is at line 60
        log.writeSidecar( sidecar( {
            entry( 60, AnnotationFile::lineDigest( lineText( 99 ) ), "moved comment" ),
        } ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "the comment follows its anchor instead of the stated line" )
        {
            REQUIRE( result.annotations.size() == 1 );
            REQUIRE( result.annotations.count( 59_lnum ) == 0 );
            REQUIRE( result.annotations.at( 99_lnum ) == "moved comment" );
            REQUIRE( result.relocated == 1 );
            REQUIRE( result.dropped == 0 );
        }
    }

    GIVEN( "a sidecar whose anchor is nowhere in the log" )
    {
        LogFixture log;

        log.writeSidecar( sidecar( {
            entry( 5, AnnotationFile::lineDigest( "a line this log never had" ), "stale" ),
            entry( 6, AnnotationFile::lineDigest( lineText( 5 ) ), "still good" ),
        } ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "it is dropped rather than shown against the wrong line" )
        {
            REQUIRE( result.dropped == 1 );
            REQUIRE( result.annotations.size() == 1 );
            REQUIRE( result.annotations.at( 5_lnum ) == "still good" );
        }
    }

    GIVEN( "a sidecar with anchors far beyond the relocation window" )
    {
        LogFixture log;

        // Line 1 holds the text, but the comment claims line 350: further than
        // the window looks, so the anchor cannot be found
        log.writeSidecar( sidecar( {
            entry( 350, AnnotationFile::lineDigest( lineText( 0 ) ), "too far to find" ),
        } ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "the comment is dropped" )
        {
            REQUIRE( result.annotations.empty() );
            REQUIRE( result.dropped == 1 );
        }
    }

    GIVEN( "a sidecar entry with no anchor" )
    {
        LogFixture log;

        log.writeSidecar( sidecar( {
            entry( 7, QString{}, "trusted blindly" ),
            entry( NbLines + 50, QString{}, "past the end" ),
        } ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "it is taken at its word, unless it is outside the file" )
        {
            REQUIRE( result.annotations.size() == 1 );
            REQUIRE( result.annotations.at( 6_lnum ) == "trusted blindly" );
            REQUIRE( result.dropped == 1 );
        }
    }

    GIVEN( "a sidecar with entries that carry no text" )
    {
        LogFixture log;

        log.writeSidecar( sidecar( {
            entry( 3, QString{}, "" ),
            entry( 4, QString{}, "kept" ),
        } ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "the empty one is skipped and not counted as a failure" )
        {
            REQUIRE( result.annotations.size() == 1 );
            REQUIRE( result.annotations.at( 3_lnum ) == "kept" );
            REQUIRE( result.dropped == 0 );
        }
    }

    GIVEN( "a malformed sidecar" )
    {
        LogFixture log;
        log.writeSidecar( QStringLiteral( "{not json" ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "an error is reported and nothing is applied" )
        {
            REQUIRE_FALSE( result.read );
            REQUIRE_FALSE( result.error.isEmpty() );
            REQUIRE( result.annotations.empty() );
        }
    }

    GIVEN( "a sidecar written by a newer klogg" )
    {
        LogFixture log;
        log.writeSidecar( sidecar( { entry( 3, QString{}, "from the future" ) },
                                   AnnotationFile::FormatVersion + 1 ) );

        const auto result = AnnotationFile::load( log.sidecarPath(), log.logData );

        THEN( "it is refused rather than guessed at" )
        {
            REQUIRE_FALSE( result.read );
            REQUIRE_FALSE( result.error.isEmpty() );
            REQUIRE( result.annotations.empty() );
        }
    }
}
