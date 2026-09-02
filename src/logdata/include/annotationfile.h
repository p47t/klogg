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

#ifndef ANNOTATIONFILE_H
#define ANNOTATIONFILE_H

#include <map>

#include <QString>

#include "linetypes.h"

class AbstractLogData;

// Reads annotations produced outside klogg from a sidecar file sitting next to
// the log file, so that another program - a script, an analysis tool, an AI
// agent - can comment on a log without knowing anything about klogg internals.
//
// The sidecar of /var/log/app.log is /var/log/app.log.klogg-annotations.json:
//
//   {
//     "version": 1,
//     "annotations": [
//       { "line": 42, "sha1": "<hex>", "text": "connection pool exhausted" },
//       { "line": 87, "text": "no anchor, trusted as is" }
//     ]
//   }
//
// "line" is 1-based, the number the log viewer and every other tool shows.
// "sha1" is the SHA-1 of the line's UTF-8 bytes without its line terminator,
// and is optional. It anchors the comment to the text rather than to a
// position: when the line has moved, the comment is relocated to wherever the
// matching text now is, and when it cannot be found the comment is dropped
// rather than shown against the wrong line.
namespace AnnotationFile {

// Highest format version this build understands
constexpr int FormatVersion = 1;

using AnnotationMap = std::map<LineNumber, QString>;

// Sidecar path for the given log file
QString pathForLogFile( const QString& logFilePath );

// SHA-1 of a line, in the form the sidecar is expected to carry
QString lineDigest( const QString& lineText );

struct LoadResult {
    // Resolved annotations, keyed by 0-based line number in the log
    AnnotationMap annotations;

    // The sidecar existed and could be parsed
    bool read = false;
    // Set when the sidecar exists but could not be used
    QString error;

    // Comments whose anchor matched somewhere other than the stated line
    size_t relocated = 0;
    // Comments whose anchor matched nowhere near the stated line
    size_t dropped = 0;
};

// Read the sidecar and resolve every anchor against logData. A missing sidecar
// is not an error: the result is simply empty with read == false.
LoadResult load( const QString& sidecarPath, const AbstractLogData& logData );

} // namespace AnnotationFile

#endif
