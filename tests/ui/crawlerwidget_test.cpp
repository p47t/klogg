/*
 * Copyright (C) 2016 -- 2019 Anton Filimonov and other contributors
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

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>
#include <qglobal.h>
#include <qnamespace.h>
#include <qtestmouse.h>

#include "savedsearches.h"
#include "session.h"
#include "test_utils.h"

#include "logdata.h"
#include "logfiltereddata.h"

#include "crawlerwidget.h"

static const qint64 SL_NB_LINES = 100LL;

namespace {
bool generateDataFiles( QTemporaryFile& file )
{
    char newLine[ 90 ];

    if ( file.open() ) {
        for ( int i = 0; i < SL_NB_LINES; i++ ) {
            snprintf( newLine, 89,
                      "LOGDATA \t is a part of glogg, we are going to test it thoroughly, this is "
                      "line %06d",
                      i );
            file.write( newLine, static_cast<qint64>( qstrlen( newLine ) ) );
#ifdef Q_OS_WIN
            file.write( "\r\n", 2 );
#else
            file.write( "\n", 1 );
#endif
        }
        file.flush();
    }

    return true;
}

} // namespace

struct CrawlerWidgetPrivate {
};

template <>
struct CrawlerWidget::access_by<CrawlerWidgetPrivate> {
    std::unique_ptr<CrawlerWidget> crawler;

    bool isLoadingFinished()
    {
        return !crawler->loadingInProgress_;
    }

    LinesCount getLogNbLines()
    {
        return crawler->logData_->getNbLine();
    }

    LinesCount getLogFilteredNbLines()
    {
        return crawler->logFilteredData_->getNbLine();
    }

    void selectAllInMainView()
    {
        crawler->logMainView_->selectAll();
    }

    void selectAllInFilteredView()
    {
        crawler->filteredView_->selectAll();
    }

    QString mainViewSelectedText()
    {
        return crawler->logMainView_->getSelectedText();
    }

    QString filteredViewSelectedText()
    {
        return crawler->filteredView_->getSelectedText();
    }

    void setSearchPattern( const QString& pattern )
    {
        QTest::keyClicks( crawler->searchLineEdit_, pattern );
    }

    void enableCaseSensitiveSearch()
    {
        if ( !crawler->matchCaseButton_->isChecked() ) {
            QTest::mouseClick( crawler->matchCaseButton_, Qt::LeftButton );
            QTest::qWait( 100 );
        }
    }

    void enableInverseMatch()
    {
        if ( !crawler->inverseButton_->isChecked() ) {
            QTest::mouseClick( crawler->inverseButton_, Qt::LeftButton );
            QTest::qWait( 100 );
        }
    }

    void enableBooleanCombinationMode()
    {
        if ( !crawler->booleanButton_->isChecked() ) {
            QTest::mouseClick( crawler->booleanButton_, Qt::LeftButton );
            QTest::qWait( 100 );
        }
    }

    void runSearch()
    {
        QTest::mouseClick( crawler->searchButton_, Qt::LeftButton );

        QTest::qWait( 100 );

        waitUiState( [ & ]() { return crawler->stopButton_->isHidden(); } );
    }

    void render()
    {
        crawler->grab();
    }

    QImage grabMainView()
    {
        return crawler->logMainView_->grab().toImage();
    }

    void annotate( LineNumber line, const QString& text )
    {
        crawler->annotateLineFromMain( line, text );
    }

    void toggleAnnotations()
    {
        crawler->toggleAnnotationsVisibility();
    }

    bool annotationsVisible() const
    {
        return crawler->annotationsVisible_;
    }

    QString annotationOfLine( LineNumber line )
    {
        return crawler->logFilteredData_->annotationByLine( line );
    }

    void setTextWrap( bool wrap )
    {
        crawler->logMainView_->textWrapSet( wrap );
    }

    QString annotationFilePath()
    {
        return crawler->annotationFilePath_;
    }

    void reloadAnnotationFile()
    {
        crawler->loadAnnotationFile();
    }

    QString viewContext()
    {
        return crawler->doGetViewContext()->toString();
    }

    QString filteredViewTextWithAnnotations()
    {
        return crawler->filteredView_->getSelectedTextWithAnnotations();
    }

    QString mainViewTextWithAnnotations()
    {
        return crawler->logMainView_->getSelectedTextWithAnnotations();
    }

    // The context menu entries are private slots, so they are triggered through
    // the meta object the same way the actions themselves trigger them
    bool copyFilteredViewSnapshot()
    {
        return QMetaObject::invokeMethod( crawler->filteredView_, "copySnapshot" );
    }
};

using CrawlerWidgetVisitor = CrawlerWidget::access_by<CrawlerWidgetPrivate>;

SCENARIO( "Crawler widget search", "[ui]" )
{
    QTemporaryFile file{ "crawler_test_XXXXXX" };
    REQUIRE( generateDataFiles( file ) );

    Session session;
    session.savedSearches().clear();

    REQUIRE( session.savedSearches().recentSearches().empty() );

    CrawlerWidgetVisitor crawlerVisitor;
    crawlerVisitor.crawler.reset( static_cast<CrawlerWidget*>(
        session.open( file.fileName(), []() { return new CrawlerWidget(); } ) ) );

    waitUiState( [ & ]() { return crawlerVisitor.getLogNbLines().get() == SL_NB_LINES; } );
    waitUiState( [ & ]() { return crawlerVisitor.isLoadingFinished(); } );

    crawlerVisitor.render();

    REQUIRE( crawlerVisitor.getLogNbLines().get() == SL_NB_LINES );

    GIVEN( "loaded log data" )
    {
        THEN( "Has no lines in log view" )
        {
            REQUIRE( crawlerVisitor.getLogFilteredNbLines().get() == 0 );
        }

        WHEN( "search for lines" )
        {
            crawlerVisitor.setSearchPattern( "this is line" );
            crawlerVisitor.runSearch();

            REQUIRE( waitUiState( [ &crawlerVisitor ]() {
                return crawlerVisitor.getLogFilteredNbLines().get() == SL_NB_LINES;
            } ) );

            THEN( "all lines are matched" )
            {
                REQUIRE( crawlerVisitor.getLogFilteredNbLines().get() == SL_NB_LINES );
            }

            AND_WHEN( "copy all from main view" )
            {
                crawlerVisitor.selectAllInMainView();
                auto text = crawlerVisitor.mainViewSelectedText();
                THEN( "text has same number of lines" )
                {
                    REQUIRE( text.split( QChar::LineFeed ).size() == SL_NB_LINES );
                }
            }

            AND_WHEN( "copy all from filtered view" )
            {
                crawlerVisitor.selectAllInFilteredView();
                auto text = crawlerVisitor.filteredViewSelectedText();
                THEN( "text has same number of lines" )
                {
                    REQUIRE( text.split( QChar::LineFeed ).size() == SL_NB_LINES );
                }
            }
        }

        WHEN( "search for 10" )
        {
            crawlerVisitor.setSearchPattern( "10" );

            crawlerVisitor.runSearch();

            waitUiState( [ & ]() { return crawlerVisitor.getLogFilteredNbLines().get() == 1; } );

            THEN( "single line match" )
            {
                REQUIRE( crawlerVisitor.getLogFilteredNbLines().get() == 1 );
            }
        }

        WHEN( "case sensitive search" )
        {
            crawlerVisitor.setSearchPattern( "THIS" );
            crawlerVisitor.enableCaseSensitiveSearch();
            crawlerVisitor.runSearch();

            THEN( "no lines matched" )
            {
                REQUIRE( crawlerVisitor.getLogFilteredNbLines().get() == 0 );
            }
        }

        WHEN( "inverse match search" )
        {
            crawlerVisitor.setSearchPattern( "not match" );
            crawlerVisitor.enableInverseMatch();
            crawlerVisitor.runSearch();

            THEN( "all lines matched" )
            {
                REQUIRE( crawlerVisitor.getLogFilteredNbLines().get() == SL_NB_LINES );
            }
        }

        WHEN( "annotating a line" )
        {
            crawlerVisitor.crawler->resize( 900, 600 );

            const auto withoutAnnotation = crawlerVisitor.grabMainView();

            crawlerVisitor.annotate( 3_lnum, "needs investigation" );
            const auto withAnnotation = crawlerVisitor.grabMainView();

            THEN( "the comment is stored for that line" )
            {
                REQUIRE( crawlerVisitor.annotationOfLine( 3_lnum ) == "needs investigation" );
            }

            THEN( "the main view draws something it did not draw before" )
            {
                REQUIRE( withAnnotation != withoutAnnotation );
            }

            AND_WHEN( "annotations are hidden" )
            {
                crawlerVisitor.toggleAnnotations();

                THEN( "the label is gone but the line stays marked as annotated" )
                {
                    REQUIRE_FALSE( crawlerVisitor.annotationsVisible() );

                    const auto hidden = crawlerVisitor.grabMainView();
                    // The label is no longer drawn over the line...
                    REQUIRE( hidden != withAnnotation );
                    // ...but the bullet keeps the line discoverable, so the
                    // view does not go back to looking un-annotated.
                    REQUIRE( hidden != withoutAnnotation );
                }

                AND_WHEN( "annotations are shown again" )
                {
                    crawlerVisitor.toggleAnnotations();

                    THEN( "the annotation is drawn again" )
                    {
                        REQUIRE( crawlerVisitor.annotationsVisible() );
                        REQUIRE( crawlerVisitor.grabMainView() == withAnnotation );
                    }
                }
            }

            AND_WHEN( "text wrap splits the line over several rows" )
            {
                crawlerVisitor.annotate( 3_lnum, QString{} );
                crawlerVisitor.setTextWrap( true );
                const auto wrappedWithout = crawlerVisitor.grabMainView();

                crawlerVisitor.annotate( 3_lnum, "needs investigation" );
                const auto wrappedWith = crawlerVisitor.grabMainView();

                THEN( "the annotation is still drawn" )
                {
                    REQUIRE( wrappedWith != wrappedWithout );
                }

                THEN( "the line is laid out differently than without it" )
                {
                    // The first row is wrapped early to leave room for the
                    // label, so the wrapped text itself changes too
                    REQUIRE( wrappedWith != withAnnotation );
                }

                crawlerVisitor.setTextWrap( false );
            }

            AND_WHEN( "the comment is cleared" )
            {
                crawlerVisitor.annotate( 3_lnum, QString{} );

                THEN( "the annotation is gone from the data and the view" )
                {
                    REQUIRE( crawlerVisitor.annotationOfLine( 3_lnum ).isEmpty() );
                    REQUIRE( crawlerVisitor.grabMainView() == withoutAnnotation );
                }
            }
        }

        WHEN( "copying annotated lines out of the filtered view" )
        {
            crawlerVisitor.crawler->resize( 900, 600 );
            crawlerVisitor.setSearchPattern( "this is line" );
            crawlerVisitor.runSearch();

            REQUIRE( waitUiState( [ &crawlerVisitor ]() {
                return crawlerVisitor.getLogFilteredNbLines().get() == SL_NB_LINES;
            } ) );

            crawlerVisitor.annotate( 1_lnum, "first symptom" );
            crawlerVisitor.annotate( 2_lnum, "root cause" );

            crawlerVisitor.selectAllInFilteredView();
            // The export uses the platform line ending, drop the CR so the
            // expectations below read the same everywhere
            const auto text
                = crawlerVisitor.filteredViewTextWithAnnotations().remove( QChar::CarriageReturn );
            const auto lines = text.split( QChar::LineFeed );

            THEN( "each comment follows the line it is attached to" )
            {
                // Line numbers are 1 based, so index 1 is line 2. The comment
                // lands on its own line right after it, indented past the
                // number column
                REQUIRE( lines[ 1 ]
                         == "  2: LOGDATA \t is a part of glogg, we are going to "
                            "test it thoroughly, this is line 000001" );
                REQUIRE( lines[ 2 ] == "     >> first symptom" );
                REQUIRE( lines[ 4 ] == "     >> root cause" );
            }

            THEN( "unannotated lines are copied with their line number only" )
            {
                REQUIRE( lines[ 0 ]
                         == "  1: LOGDATA \t is a part of glogg, we are going to "
                            "test it thoroughly, this is line 000000" );
                REQUIRE_FALSE( lines[ 0 ].contains( ">>" ) );
            }

            THEN( "the export has a row per line plus one per comment" )
            {
                REQUIRE( lines.size() == SL_NB_LINES + 2 );
            }

            AND_WHEN( "the same lines are copied from the main view" )
            {
                crawlerVisitor.selectAllInMainView();

                THEN( "the comments are attached to the same file lines" )
                {
                    const auto mainText = crawlerVisitor.mainViewTextWithAnnotations().remove(
                        QChar::CarriageReturn );
                    REQUIRE( mainText.split( QChar::LineFeed )[ 2 ] == "     >> first symptom" );
                }
            }
        }

        WHEN( "copying a snapshot of the filtered view" )
        {
            crawlerVisitor.crawler->resize( 900, 600 );
            crawlerVisitor.setSearchPattern( "this is line" );
            crawlerVisitor.runSearch();

            REQUIRE( waitUiState( [ &crawlerVisitor ]() {
                return crawlerVisitor.getLogFilteredNbLines().get() == SL_NB_LINES;
            } ) );

            crawlerVisitor.annotate( 1_lnum, "first symptom" );
            crawlerVisitor.render();

            auto* clipboard = QApplication::clipboard();
            REQUIRE( clipboard != nullptr );
            clipboard->clear();

            REQUIRE( crawlerVisitor.copyFilteredViewSnapshot() );

            THEN( "the clipboard holds a picture of the panel" )
            {
                const auto image = clipboard->image();
                REQUIRE_FALSE( image.isNull() );
                REQUIRE( image.width() > 0 );
                REQUIRE( image.height() > 0 );
            }
        }

        WHEN( "an external program writes the annotation sidecar" )
        {
            const auto sidecarPath = crawlerVisitor.annotationFilePath();
            REQUIRE( sidecarPath == file.fileName() + ".klogg-annotations.json" );

            // QTemporaryFile only cleans up the log itself, so the sidecar
            // written next to it has to be removed by hand
            struct SidecarCleanup {
                QString path;
                ~SidecarCleanup()
                {
                    QFile::remove( path );
                }
            } cleanup{ sidecarPath };

            const auto writeSidecar = [ &sidecarPath ]( const QString& contents ) {
                QFile sidecar{ sidecarPath };
                REQUIRE( sidecar.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
                sidecar.write( contents.toUtf8() );
                sidecar.close();
            };

            writeSidecar( R"({"version":1,"annotations":[
                {"line":4,"text":"found by an external tool"}]})" );
            crawlerVisitor.reloadAnnotationFile();

            THEN( "the comment shows up on the line it names" )
            {
                REQUIRE( crawlerVisitor.annotationOfLine( 3_lnum ) == "found by an external tool" );
            }

            AND_WHEN( "a comment is added in klogg itself" )
            {
                crawlerVisitor.annotate( 10_lnum, "mine" );

                AND_WHEN( "the sidecar is rewritten without its earlier entry" )
                {
                    writeSidecar( R"({"version":1,"annotations":[
                        {"line":8,"text":"a different line now"}]})" );
                    crawlerVisitor.reloadAnnotationFile();

                    THEN( "the sidecar's own comments are replaced" )
                    {
                        REQUIRE( crawlerVisitor.annotationOfLine( 3_lnum ).isEmpty() );
                        REQUIRE( crawlerVisitor.annotationOfLine( 7_lnum )
                                 == "a different line now" );
                    }

                    THEN( "the comment written in klogg is left alone" )
                    {
                        REQUIRE( crawlerVisitor.annotationOfLine( 10_lnum ) == "mine" );
                    }
                }
            }

            AND_WHEN( "the sidecar is removed" )
            {
                REQUIRE( QFile::remove( sidecarPath ) );
                crawlerVisitor.reloadAnnotationFile();

                THEN( "its comments go away" )
                {
                    REQUIRE( crawlerVisitor.annotationOfLine( 3_lnum ).isEmpty() );
                }
            }

            AND_WHEN( "the session is saved" )
            {
                crawlerVisitor.annotate( 10_lnum, "written in klogg" );
                const auto context = crawlerVisitor.viewContext();

                THEN( "comments written in klogg are stored" )
                {
                    REQUIRE( context.contains( "written in klogg" ) );
                }

                THEN( "comments owned by the sidecar are not" )
                {
                    // The sidecar is their source of truth; keeping a second
                    // copy would resurrect entries it later drops
                    REQUIRE_FALSE( context.contains( "found by an external tool" ) );
                }

                THEN( "the mark implied by a sidecar comment is not saved either" )
                {
                    // Line 4 in the sidecar is index 3, and the only mark in the
                    // session should be the one implied by the klogg comment on
                    // line index 10
                    REQUIRE( context.contains( "\"M\":[10]" ) );
                }
            }
        }

        WHEN( "boolean search" )
        {
            crawlerVisitor.setSearchPattern( "\"glogg\" or \"klogg\"" );
            crawlerVisitor.enableBooleanCombinationMode();
            crawlerVisitor.runSearch();

            THEN( "has lines matched" )
            {
                REQUIRE( crawlerVisitor.getLogFilteredNbLines().get() >= 2 );
            }
        }
    }
}
