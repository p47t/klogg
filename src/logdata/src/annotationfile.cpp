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

#include "annotationfile.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "abstractlogdata.h"
#include "log.h"

namespace {

// How far from the stated line a moved anchor is looked for. Appending to a log
// does not move earlier lines at all, so this only has to cover edits and
// re-generated files, and it bounds the reading a bad sidecar can cause.
constexpr uint64_t RelocationWindow = 200;

// Upper bound on relocation searches for one load, so that a sidecar written
// against an unrelated file cannot turn into a full scan per comment.
constexpr size_t RelocationBudget = 500;

// Look for lineDigest around statedLine and return where it was found
OptionalLineNumber findByDigest( const AbstractLogData& logData, LineNumber statedLine,
                                 const QString& digest )
{
    const auto nbLines = logData.getNbLine().get();
    if ( nbLines == 0 ) {
        return {};
    }

    const auto stated = statedLine.get();
    const auto first = stated > RelocationWindow ? stated - RelocationWindow : uint64_t{ 0 };
    const auto last = std::min( stated + RelocationWindow, nbLines - 1 );

    const auto lines = logData.getLines( LineNumber( first ), LinesCount( last - first + 1 ) );

    // Prefer the candidate closest to where the comment said it was
    OptionalLineNumber found;
    uint64_t bestDistance = 0;
    for ( size_t index = 0; index < lines.size(); ++index ) {
        if ( AnnotationFile::lineDigest( lines[ index ] ) != digest ) {
            continue;
        }

        const auto candidate = first + index;
        const auto distance = candidate > stated ? candidate - stated : stated - candidate;
        if ( !found.has_value() || distance < bestDistance ) {
            found = LineNumber( candidate );
            bestDistance = distance;
        }
    }

    return found;
}

} // namespace

QString AnnotationFile::pathForLogFile( const QString& logFilePath )
{
    return logFilePath + QLatin1String( ".klogg-annotations.json" );
}

QString AnnotationFile::lineDigest( const QString& lineText )
{
    return QString::fromLatin1(
        QCryptographicHash::hash( lineText.toUtf8(), QCryptographicHash::Sha1 ).toHex() );
}

AnnotationFile::LoadResult AnnotationFile::load( const QString& sidecarPath,
                                                 const AbstractLogData& logData )
{
    LoadResult result;

    QFile sidecar{ sidecarPath };
    if ( !sidecar.exists() ) {
        return result;
    }

    if ( !sidecar.open( QIODevice::ReadOnly ) ) {
        result.error = sidecar.errorString();
        return result;
    }

    const auto content = sidecar.readAll();
    sidecar.close();

    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson( content, &parseError );
    if ( parseError.error != QJsonParseError::NoError ) {
        result.error = parseError.errorString();
        return result;
    }

    if ( !document.isObject() ) {
        result.error = QLatin1String( "expected a JSON object at the top level" );
        return result;
    }

    const auto root = document.object();

    const auto version = root.value( QLatin1String( "version" ) ).toInt( FormatVersion );
    if ( version > FormatVersion ) {
        result.error = QStringLiteral( "unsupported format version %1" ).arg( version );
        return result;
    }

    result.read = true;

    const auto nbLines = logData.getNbLine().get();
    size_t relocationSearches = 0;

    const auto entries = root.value( QLatin1String( "annotations" ) ).toArray();
    for ( const auto& entry : entries ) {
        const auto fields = entry.toObject();

        const auto text = fields.value( QLatin1String( "text" ) ).toString().simplified();
        if ( text.isEmpty() ) {
            continue;
        }

        // The file speaks in 1-based line numbers, like every other tool
        const auto statedLine = fields.value( QLatin1String( "line" ) ).toInteger( 0 );
        if ( statedLine < 1 ) {
            LOG_WARNING << "Annotation sidecar: line " << statedLine << " is out of range";
            ++result.dropped;
            continue;
        }

        const auto line = LineNumber( static_cast<LineNumber::UnderlyingType>( statedLine - 1 ) );
        const auto digest = fields.value( QLatin1String( "sha1" ) ).toString().toLower();

        if ( digest.isEmpty() ) {
            // No anchor to check, take the line number at its word
            if ( line.get() >= nbLines ) {
                LOG_WARNING << "Annotation sidecar: unanchored comment for line " << statedLine
                            << " is past the end of the file";
                ++result.dropped;
                continue;
            }

            result.annotations[ line ] = text;
            continue;
        }

        const auto lineMatches
            = line.get() < nbLines && lineDigest( logData.getLineString( line ) ) == digest;
        if ( lineMatches ) {
            result.annotations[ line ] = text;
            continue;
        }

        if ( relocationSearches >= RelocationBudget ) {
            LOG_WARNING << "Annotation sidecar: relocation budget spent, dropping comment for line "
                        << statedLine;
            ++result.dropped;
            continue;
        }

        ++relocationSearches;
        const auto relocated = findByDigest( logData, line, digest );
        if ( !relocated.has_value() ) {
            LOG_WARNING
                << "Annotation sidecar: no line matching the anchor of the comment for line "
                << statedLine;
            ++result.dropped;
            continue;
        }

        LOG_INFO << "Annotation sidecar: comment for line " << statedLine << " relocated to line "
                 << relocated->get() + 1;
        result.annotations[ *relocated ] = text;
        ++result.relocated;
    }

    LOG_INFO << "Annotation sidecar " << sidecarPath << ": " << result.annotations.size()
             << " comments, " << result.relocated << " relocated, " << result.dropped << " dropped";

    return result;
}
