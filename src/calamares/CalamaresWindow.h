/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2018 Raul Rodrigo Segura (raurodse)
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-FileCopyrightText: 2020 Anubhav Choudhary <ac.10edu@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef CALAMARESWINDOW_H
#define CALAMARESWINDOW_H

#include <QWidget>

#include <memory>

class QLabel;
class QPushButton;

namespace Calamares
{
class DebugWindowManager;
class ViewManager;
}  // namespace Calamares

class PageIndicator;

/**
 * @brief The CalamaresWindow class represents the main window of the Calamares UI.
 *
 * With branding *windowExpanding* set to "fullscreen" the window is a
 * frameless full-screen shell that visually replicates deepin-installer:
 * a wallpaper background, a close button in the top-right corner, a large
 * page title, the ViewManager pages and a row of page-indicator dots at
 * the bottom, with the navigation buttons just above them.
 */
class CalamaresWindow : public QWidget
{
    Q_OBJECT
public:
    CalamaresWindow( QWidget* parent = nullptr );
    ~CalamaresWindow() override {}

public Q_SLOTS:
    /**
     * This asks the main window to grow to accomodate @p size pixels, to accomodate
     * larger-than-expected window contents. The enlargement may be silently
     * ignored.
     */
    void ensureSize( QSize size );

protected:
    virtual void closeEvent( QCloseEvent* e ) override;
    virtual void resizeEvent( QResizeEvent* e ) override;

private Q_SLOTS:
    ///@brief A view step's internal editor page opened/closed
    void onInternalPageActive( bool active );

private:
    ///@brief Build the deepin-installer-like full-screen shell.
    void initFullscreenUI();
    ///@brief Refresh title, page indicator and button visibility for the current step.
    void updateFullscreenPage();
    ///@brief Hook a view step's internal-page signal, and refresh
    void connectStepInternalPage( int row );

private:
    ///@brief Scale the wallpaper over the whole window, deepin style.
    void updateBackground();

    Calamares::DebugWindowManager* m_debugManager = nullptr;
    Calamares::ViewManager* m_viewManager = nullptr;

    ///@brief True when branding selects "fullscreen" expansion (deepin shell).
    bool m_fullscreenMode = false;
    ///@brief A view step has an internal editor page open (hide back/next).
    bool m_internalPageActive = false;

    // Fullscreen (deepin) shell widgets
    QLabel* m_background = nullptr;
    QPushButton* m_closeButton = nullptr;
    QPushButton* m_aboutButton = nullptr;
    QPushButton* m_debugButton = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subTitleLabel = nullptr;
    QWidget* m_navRow = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_restartButton = nullptr;
    QPushButton* m_continueButton = nullptr;
    PageIndicator* m_pageIndicator = nullptr;
    QWidget* m_indicatorWrapper = nullptr;
};

#endif  // CALAMARESWINDOW_H
