/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   PageIndicator ported from deepin-installer-reborn (GPL-3.0+),
 *   Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "PageIndicator.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSvgRenderer>

namespace
{
///@brief Render an SVG resource at a HiDPI-aware size, like deepin's renderPixmap().
QPixmap
renderSvg( const QString& path, int size )
{
    const qreal ratio = qApp ? qApp->devicePixelRatio() : 1.0;
    QSvgRenderer renderer( path );
    QPixmap pixmap( int( size * ratio ), int( size * ratio ) );
    if ( renderer.isValid() )
    {
        pixmap.fill( Qt::transparent );
        QPainter painter( &pixmap );
        renderer.render( &painter, QRectF( 0, 0, size * ratio, size * ratio ) );
        painter.end();
    }
    pixmap.setDevicePixelRatio( ratio );
    return pixmap;
}
}  // namespace

PageIndicator::PageIndicator( int pages, QWidget* parent )
    : QFrame( parent )
    , m_pages( pages )
    , m_currentPage( -1 )
    , m_indicatorActive( renderSvg( QStringLiteral( ":/deepin/indicator_active.svg" ), 13 ) )
    , m_indicatorInactive( renderSvg( QStringLiteral( ":/deepin/indicator_inactive.svg" ), 13 ) )
{
    setObjectName( QStringLiteral( "pageIndicator" ) );
    initUI();
}

void
PageIndicator::goNextPage()
{
    setCurrentPage( m_currentPage + 1 );
}

void
PageIndicator::setCurrentPage( int page )
{
    m_currentPage = page;
    updateIndicators();
}

void
PageIndicator::updatePages( int pages )
{
    m_pages = pages;

    for ( QLabel* w : m_indicators )
    {
        m_layout->removeWidget( w );
        w->deleteLater();
    }
    m_indicators.clear();

    for ( int i = 0; i != m_pages; ++i )
    {
        QLabel* label = new QLabel;
        label->setFixedSize( 13, 13 );
        label->setPixmap( m_indicatorInactive );
        m_layout->addWidget( label );
        m_indicators << label;
    }

    updateIndicators();
}

void
PageIndicator::initUI()
{
    m_layout = new QHBoxLayout();
    m_layout->setContentsMargins( 0, 0, 0, 0 );
    m_layout->setSpacing( 0 );
    m_layout->setAlignment( Qt::AlignCenter );

    updatePages( m_pages );

    setLayout( m_layout );
}

void
PageIndicator::updateIndicators()
{
    for ( int i = 0; i < m_pages; i++ )
    {
        QLabel* label = m_indicators[ i ];
        label->setPixmap( i == m_currentPage ? m_indicatorActive : m_indicatorInactive );
    }
}
