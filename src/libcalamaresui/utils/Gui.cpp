/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Gui.h"

#include "Branding.h"
#include "ImageRegistry.h"

#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QLayout>
#include <QPainter>
#include <QEventLoop>
#include <QPen>
#include <QWidget>

#define RESPATH ":/data/"

namespace Calamares
{

static int s_defaultFontSize = 0;
static int s_defaultFontHeight = 0;

QPixmap
defaultPixmap( ImageType type, ImageMode mode, const QSize& size )
{
    Q_UNUSED( mode )
    QPixmap pixmap;

    switch ( type )
    {
    case Yes:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/yes.svgz", size );
        break;

    case No:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/no.svgz", size );
        break;

    case Information:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/information.svgz", size );
        break;

    case Fail:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/fail.svgz", size );
        break;

    case Bugs:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/bugs.svg", size );
        break;

    case Help:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/help.svg", size );
        break;

    case Release:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/release.svg", size );
        break;

    case Donate:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/donate.svg", size );
        break;

    case PartitionDisk:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-disk.svg", size );
        break;

    case PartitionPartition:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-partition.svg", size );
        break;

    case PartitionAlongside:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-alongside.svg", size );
        break;

    case PartitionEraseAuto:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-erase-auto.svg", size );
        break;

    case PartitionManual:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-manual.svg", size );
        break;

    case PartitionReplaceOs:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-replace-os.svg", size );
        break;

    case PartitionTable:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/partition-table.svg", size );
        break;

    case BootEnvironment:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/boot-environment.svg", size );
        break;

    case Squid:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/squid.svg", size );
        break;

    case StatusOk:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/state-ok.svg", size );
        break;

    case StatusWarning:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/state-warning.svg", size );
        break;

    case StatusError:
        pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/state-error.svg", size );
        break;
    }

    if ( pixmap.isNull() )
    {
        Q_ASSERT( false );
        return QPixmap();
    }

    return pixmap;
}

void
unmarginLayout( QLayout* layout )
{
    if ( layout )
    {
        layout->setContentsMargins( 0, 0, 0, 0 );
        layout->setSpacing( 0 );

        for ( int i = 0; i < layout->count(); i++ )
        {
            auto* childItem = layout->itemAt( i );
            QLayout* childLayout = childItem ? childItem->layout() : nullptr;
            if ( childLayout )
            {
                unmarginLayout( childLayout );
            }
        }
    }
}

int
defaultFontSize()
{
    if ( s_defaultFontSize <= 0 )
    {
        s_defaultFontSize = QFont().pointSize();
    }
    return s_defaultFontSize;
}

int
defaultFontHeight()
{
    if ( s_defaultFontHeight <= 0 )
    {
        QFont f;
        f.setPointSize( defaultFontSize() );
        s_defaultFontHeight = QFontMetrics( f ).height();
    }

    return s_defaultFontHeight;
}

QFont
largeFont()
{
    QFont f;
    f.setPointSize( defaultFontSize() + 4 );
    return f;
}

void
setDefaultFontSize( int points )
{
    s_defaultFontSize = points;
    s_defaultFontHeight = 0;  // Recalculate on next call to defaultFontHeight()
}

QSize
defaultIconSize()
{
    const int w = int( defaultFontHeight() * 1.6 );
    return QSize( w, w );
}

}  // namespace Calamares

int
Calamares::presentAsFullscreen( QDialog* dialog )
{
    if ( !dialog )
    {
        return QDialog::Rejected;
    }

    const auto* branding = Branding::instance();
    const bool fullscreenBranding = branding && branding->windowMaximize();
    QWidget* mainWindow = dialog->parentWidget() ? dialog->parentWidget()->window() : nullptr;
    if ( fullscreenBranding && mainWindow )
    {
        // deepin-installer style: the dialog becomes a dedicated page of
        // the installer. Reparent it into the main window as a plain
        // child widget covering the whole window: as a child widget its
        // translucent background composes with the wallpaper of the main
        // window (a top-level translucent dialog would just paint black
        // without a compositor).
        dialog->setParent( mainWindow );
        dialog->setWindowFlag( Qt::Dialog, false );
        dialog->setGeometry( mainWindow->rect() );
        dialog->setStyleSheet( QStringLiteral( "QDialog { background: rgba(0, 0, 0, 0.45); }" ) );
        if ( dialog->layout() )
        {
            const int m = 3 * defaultFontHeight();
            dialog->layout()->setContentsMargins( m, m / 2, m, m / 2 );
        }
        dialog->show();
        dialog->raise();

        // Wait until the dialog is done (accept/reject), like exec()
        QEventLoop loop;
        QObject::connect( dialog, &QDialog::finished, &loop, &QEventLoop::quit );
        loop.exec();
        return dialog->result();
    }
    return dialog->exec();
}
