/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   PageIndicator ported from deepin-installer-reborn (GPL-3.0+),
 *   Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef CALAMARES_PAGEINDICATOR_H
#define CALAMARES_PAGEINDICATOR_H

#include <QFrame>
#include <QPixmap>

class QHBoxLayout;
class QLabel;

/** @brief Display progress through pages as a row of dots.
 *
 * Visual replica of the deepin-installer page indicator: a row of
 * small circles, one per page, the active one highlighted.
 */
class PageIndicator : public QFrame
{
    Q_OBJECT

public:
    explicit PageIndicator( int pages, QWidget* parent = nullptr );

    ///@brief Number of pages (dots) displayed.
    int pages() const { return m_pages; }

public Q_SLOTS:
    ///@brief Change the number of dots; current page is reset.
    void updatePages( int pages );
    ///@brief Highlight the @p page -th dot (0-based).
    void setCurrentPage( int page );
    ///@brief Convenience: advance one dot.
    void goNextPage();

private:
    void initUI();
    void updateIndicators();

    int m_pages = 0;
    int m_currentPage = -1;

    QHBoxLayout* m_layout = nullptr;
    QPixmap m_indicatorActive;
    QPixmap m_indicatorInactive;
    QList< QLabel* > m_indicators;
};

#endif  // CALAMARES_PAGEINDICATOR_H
