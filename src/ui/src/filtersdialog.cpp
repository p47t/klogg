/*
 * Copyright (C) 2009, 2010 Nicolas Bonnefon and other contributors
 *
 * This file is part of glogg.
 *
 * glogg is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glogg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glogg.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "log.h"

#include "configuration.h"
#include "persistentinfo.h"
#include "filterset.h"

#include "filtersdialog.h"

#include <QColorDialog>

static const char* DEFAULT_PATTERN = "New Filter";
static const bool    DEFAULT_IGNORE_CASE = false;

static const QColor DEFAULT_FORE_COLOUR("#000000");
static const QColor DEFAULT_BACK_COLOUR("#FFFFFF");

// Construct the box, including a copy of the global FilterSet
// to handle ok/cancel/apply
FiltersDialog::FiltersDialog( QWidget* parent ) : QDialog( parent )
{
    setupUi( this );

    // Reload the filter list from disk (in case it has been changed
    // by another glogg instance) and copy it to here.
    GetPersistentInfo().retrieve( "filterSet" );
    filterSet_ = Persistent<FilterSet>( "filterSet" );

    populateFilterList();

    // Start with all buttons disabled except 'add'
    removeFilterButton->setEnabled(false);
    upFilterButton->setEnabled(false);
    downFilterButton->setEnabled(false);

    // Default to black on white
    updateIcon( foreColorButton , DEFAULT_FORE_COLOUR );
    updateIcon( backColorButton , DEFAULT_BACK_COLOUR );

    foreColor_ = DEFAULT_FORE_COLOUR;
    backColor_ = DEFAULT_BACK_COLOUR;

    // No filter selected by default
    selectedRow_ = -1;

    connect( filterListWidget, SIGNAL( itemSelectionChanged() ),
            this, SLOT( updatePropertyFields() ) );
    connect( patternEdit, SIGNAL( textEdited( const QString& ) ),
            this, SLOT( updateFilterProperties() ) );
    connect( ignoreCaseCheckBox, SIGNAL( clicked(bool) ),
            this, SLOT( updateFilterProperties() ) );

    if ( !filterSet_.filterList.empty() ) {
        filterListWidget->setCurrentItem( filterListWidget->item( 0 ) );
    }
}

//
// Slots
//

void FiltersDialog::on_addFilterButton_clicked()
{
    LOG(logDEBUG) << "on_addFilterButton_clicked()";

    Filter newFilter = Filter( DEFAULT_PATTERN, DEFAULT_IGNORE_CASE,
                               DEFAULT_FORE_COLOUR, DEFAULT_BACK_COLOUR );

    filterSet_.filterList << newFilter;

    // Add and select the newly created filter
    filterListWidget->addItem( DEFAULT_PATTERN );
    filterListWidget->setCurrentRow( filterListWidget->count() - 1 );
}

void FiltersDialog::on_removeFilterButton_clicked()
{
    int index = filterListWidget->currentRow();
    LOG(logDEBUG) << "on_removeFilterButton_clicked() index " << index;

    if ( index >= 0 ) {
        filterSet_.filterList.removeAt( index );
        filterListWidget->setCurrentRow( -1 );
        delete filterListWidget->takeItem( index );

        // Select the new item at the same index
        filterListWidget->setCurrentRow( index );

        int count = filterListWidget->count();
        if ( index < count ) {
            // Select the new item at the same index
            filterListWidget->setCurrentRow( index );
        }
        else {
            // or the previous index if it is at the end
            filterListWidget->setCurrentRow( count - 1 );
        }
    }
}

void FiltersDialog::on_upFilterButton_clicked()
{
    int index = filterListWidget->currentRow();
    LOG(logDEBUG) << "on_upFilterButton_clicked() index " << index;

    if ( index > 0 ) {
        filterSet_.filterList.move( index, index - 1 );

        QListWidgetItem* item = filterListWidget->takeItem( index );
        filterListWidget->insertItem( index - 1, item );
        filterListWidget->setCurrentRow( index - 1 );
    }
}

void FiltersDialog::on_downFilterButton_clicked()
{
    int index = filterListWidget->currentRow();
    LOG(logDEBUG) << "on_downFilterButton_clicked() index " << index;

    if ( ( index >= 0 ) && ( index < ( filterListWidget->count() - 1 ) ) ) {
        filterSet_.filterList.move( index, index + 1 );

        QListWidgetItem* item = filterListWidget->takeItem( index );
        filterListWidget->insertItem( index + 1, item );
        filterListWidget->setCurrentRow( index + 1 );
    }
}

void FiltersDialog::on_buttonBox_clicked( QAbstractButton* button )
{
    LOG(logDEBUG) << "on_buttonBox_clicked()";

    QDialogButtonBox::ButtonRole role = buttonBox->buttonRole( button );
    if (   ( role == QDialogButtonBox::AcceptRole )
        || ( role == QDialogButtonBox::ApplyRole ) ) {
        // Copy the filter set and persist it to disk
        Persistent<FilterSet>( "filterSet" ) = filterSet_;
        GetPersistentInfo().save( "filterSet" );
        emit optionsChanged();
    }

    if ( role == QDialogButtonBox::AcceptRole )
        accept();
    else if ( role == QDialogButtonBox::RejectRole )
        reject();
}

void FiltersDialog::on_foreColorButton_clicked()
{
    // this method should never be called without a selected row
    // as all the property widgets should be disabled in this state
    if( selectedRow_ >= 0 ) {
        Filter& currentFilter = filterSet_.filterList[ selectedRow_ ];

        QColor new_color;
        if ( showColorPicker( currentFilter.foreColor() , new_color ) ) {
            currentFilter.setForeColor(new_color);
            updateIcon(foreColorButton , currentFilter.foreColor());
            filterListWidget->currentItem()->setForeground( QBrush( new_color ) );
            foreColor_ = new_color;
        }
    }
}

void FiltersDialog::on_backColorButton_clicked()
{
    // this method should never be called without a selected row
    // as all the property widgets should be disabled in this state
    if( selectedRow_ >= 0 ) {
        Filter& currentFilter = filterSet_.filterList[ selectedRow_ ];

        QColor new_color;
        if ( showColorPicker( currentFilter.backColor() , new_color ) ) {
            currentFilter.setBackColor(new_color);
            updateIcon(backColorButton , currentFilter.backColor());
            filterListWidget->currentItem()->setBackground( QBrush( new_color ) );
            backColor_ = new_color;
        }
    }
}

void FiltersDialog::updatePropertyFields()
{
    if ( filterListWidget->selectedItems().count() >= 1 )
        selectedRow_ = filterListWidget->row(
                filterListWidget->selectedItems().at(0) );
    else
        selectedRow_ = -1;

    LOG(logDEBUG) << "updatePropertyFields(), row = " << selectedRow_;

    if ( selectedRow_ >= 0 ) {
        const Filter& currentFilter = filterSet_.filterList.at( selectedRow_ );

        patternEdit->setText( currentFilter.pattern() );
        patternEdit->setEnabled( true );

        ignoreCaseCheckBox->setChecked( currentFilter.ignoreCase() );
        ignoreCaseCheckBox->setEnabled( true );

        updateIcon( foreColorButton , currentFilter.foreColor() );
        updateIcon( backColorButton , currentFilter.backColor() );

        foreColor_ = currentFilter.foreColor();
        backColor_ = currentFilter.backColor();

        // Enable the buttons if needed
        removeFilterButton->setEnabled( true );
        foreColorButton->setEnabled( true );
        backColorButton->setEnabled( true );
        upFilterButton->setEnabled( selectedRow_ > 0 );
        downFilterButton->setEnabled( selectedRow_ < ( filterListWidget->count() - 1 ) );
    }
    else {
        // Nothing is selected, reset and disable the controls
        patternEdit->clear();
        patternEdit->setEnabled( false );

        foreColorButton->setEnabled( false );
        backColorButton->setEnabled( false );
        updateIcon(foreColorButton , DEFAULT_FORE_COLOUR);
        updateIcon(backColorButton , DEFAULT_BACK_COLOUR);
        foreColor_ = DEFAULT_FORE_COLOUR;
        backColor_ = DEFAULT_BACK_COLOUR;

        ignoreCaseCheckBox->setChecked( DEFAULT_IGNORE_CASE );
        ignoreCaseCheckBox->setEnabled( false );
        removeFilterButton->setEnabled( false );
        upFilterButton->setEnabled( false );
        downFilterButton->setEnabled( false );
    }
}

void FiltersDialog::updateFilterProperties()
{
    LOG(logDEBUG) << "updateFilterProperties()";

    // If a row is selected
    if ( selectedRow_ >= 0 ) {
        Filter& currentFilter = filterSet_.filterList[selectedRow_];

        // Update the internal data
        currentFilter.setPattern( patternEdit->text() );
        currentFilter.setIgnoreCase( ignoreCaseCheckBox->isChecked() );
        currentFilter.setForeColor( foreColor_ );
        currentFilter.setBackColor( backColor_ );

        // Update the entry in the filterList widget
        filterListWidget->currentItem()->setText( patternEdit->text() );
        filterListWidget->currentItem()->setForeground(
                QBrush( QColor( currentFilter.foreColor() ) ) );
        filterListWidget->currentItem()->setBackground(
                QBrush( QColor( currentFilter.backColor() ) ) );
    }
}

//
// Private functions
//

void FiltersDialog::updateIcon (QPushButton* button , QColor color)
{
    QPixmap CustomPixmap( 20, 10 );
    CustomPixmap.fill( color );
    button->setIcon(QIcon( CustomPixmap ));
}

bool FiltersDialog::showColorPicker (const QColor& in , QColor& out)
{
    QColorDialog dialog;

    // non native dialog ensures they will have a default
    // set of colors to pick from in a pallette. For example,
    // on some linux desktops, the basic palette is missing
    dialog.setOption( QColorDialog::DontUseNativeDialog , true);

    dialog.setModal( true );
    dialog.setCurrentColor( in );
    dialog.exec();
    out = dialog.currentColor();

    return ( dialog.result() == QDialog::Accepted );
}

void FiltersDialog::populateFilterList()
{
    filterListWidget->clear();
    for ( const Filter& filter : qAsConst(filterSet_.filterList) ) {
        QListWidgetItem* new_item = new QListWidgetItem( filter.pattern() );
        // new_item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled );
        new_item->setForeground( QBrush( filter.foreColor() ) );
        new_item->setBackground( QBrush( filter.backColor() ) );
        filterListWidget->addItem( new_item );
    }
}
