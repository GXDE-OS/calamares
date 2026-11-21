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

#include "CalamaresWindow.h"

#include "Branding.h"
#include "CalamaresConfig.h"
#include "DebugWindow.h"
#include "PageIndicator.h"
#include "Settings.h"
#include "ViewManager.h"

#include "progresstree/ProgressTreeView.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Qml.h"
#include "utils/Retranslator.h"
#include "viewpages/ViewStep.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
#include <QDesktopWidget>
#endif
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#ifdef WITH_QML
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#endif
#include <QRegularExpression>
#include <QTreeView>

#include <algorithm>

static QSize
desktopSize( QWidget* w )
{
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
    return qApp->desktop()->availableGeometry( w ).size();
#else
    return w->screen()->availableGeometry().size();
#endif
}

static inline int
windowDimensionToPixels( const Calamares::Branding::WindowDimension& u )
{
    if ( !u.isValid() )
    {
        return 0;
    }
    if ( u.unit() == Calamares::Branding::WindowDimensionUnit::Pixies )
    {
        return static_cast< int >( u.value() );
    }
    if ( u.unit() == Calamares::Branding::WindowDimensionUnit::Fonties )
    {
        return static_cast< int >( u.value() * Calamares::defaultFontHeight() );
    }
    return 0;
}

/** @brief Expected orientation of the panels, based on their side
 *
 * Panels on the left and right are expected to be "vertical" style,
 * top and bottom should be "horizontal bars". This function maps
 * the sides to expected orientation.
 */
static inline Qt::Orientation
orientation( const Calamares::Branding::PanelSide s )
{
    using Side = Calamares::Branding::PanelSide;
    return ( s == Side::Left || s == Side::Right ) ? Qt::Orientation::Vertical : Qt::Orientation::Horizontal;
}

/** @brief Get a button-sized icon. */
static inline QPixmap
getButtonIcon( const QString& name )
{
    return Calamares::Branding::instance()->image( name, QSize( 22, 22 ) );
}

static inline void
setButtonIcon( QPushButton* button, const QString& name )
{
    auto icon = getButtonIcon( name );
    if ( button && !icon.isNull() )
    {
        button->setIcon( icon );
    }
}

/** @brief Strip button mnemonics for the deepin-style nav buttons.
 *
 * Handles both the latin "&Next" style and the CJK "后退(&B)" style,
 * so the buttons read plainly ("后退", "下一步") like deepin-installer.
 */
static QString
stripButtonMnemonic( QString text )
{
    text.remove( QRegularExpression( QStringLiteral( "\\s*\\(&[A-Za-z0-9]+\\)" ) ) );
    text.remove( QLatin1Char( '&' ) );
    return text;
}

/** @brief Enable the "Next" button and possibly auto-proceed to next page */
static inline void
setNextButtonEnabled( QPushButton* button, Calamares::ViewStep* step, bool enabled )
{
    button->setEnabled( enabled );

    if ( !enabled || !step )
    {
        return;
    }

    const auto descriptor = Calamares::Settings::instance()->moduleInstance( step->moduleInstanceKey() );
    if ( descriptor.isValid() && descriptor.autoProceed() )
    {
        button->click();
    }
}

static QWidget*
getWidgetSidebar( Calamares::DebugWindowManager* debug,
                  Calamares::ViewManager* viewManager,
                  QWidget* parent,
                  Qt::Orientation,
                  int desiredWidth )
{
    const Calamares::Branding* const branding = Calamares::Branding::instance();

    QWidget* sideBox = new QWidget( parent );
    sideBox->setObjectName( "sidebarApp" );

    QBoxLayout* sideLayout = new QVBoxLayout;
    sideBox->setLayout( sideLayout );
    // Set this attribute into qss file
    sideBox->setFixedWidth( desiredWidth );
    sideBox->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    QHBoxLayout* logoLayout = new QHBoxLayout;
    sideLayout->addLayout( logoLayout );
    logoLayout->addStretch();
    QLabel* logoLabel = new QLabel( sideBox );
    logoLabel->setObjectName( "logoApp" );
    //Define all values into qss file
    {
        QPalette plt = sideBox->palette();
        sideBox->setAutoFillBackground( true );
        plt.setColor( sideBox->backgroundRole(), branding->styleString( Calamares::Branding::SidebarBackground ) );
        plt.setColor( sideBox->foregroundRole(), branding->styleString( Calamares::Branding::SidebarText ) );
        sideBox->setPalette( plt );
        logoLabel->setPalette( plt );
    }
    logoLabel->setAlignment( Qt::AlignCenter );
    logoLabel->setFixedSize( 80, 80 );
    logoLabel->setPixmap( branding->image( Calamares::Branding::ProductLogo, logoLabel->size() ) );
    logoLayout->addWidget( logoLabel );
    logoLayout->addStretch();

    ProgressTreeView* tv = new ProgressTreeView( sideBox );
    tv->setModel( viewManager );
    tv->setFocusPolicy( Qt::NoFocus );
    sideLayout->addWidget( tv );

    QHBoxLayout* extraButtons = new QHBoxLayout;
    sideLayout->addLayout( extraButtons );

    const int defaultFontHeight = Calamares::defaultFontHeight();

    if ( /* About-Calamares Button enabled */ true )
    {
        QPushButton* aboutDialog = new QPushButton;
        aboutDialog->setObjectName( "aboutButton" );
        aboutDialog->setIcon( Calamares::defaultPixmap(
            Calamares::Information, Calamares::Original, 2 * QSize( defaultFontHeight, defaultFontHeight ) ) );
        CALAMARES_RETRANSLATE_FOR(
            aboutDialog, aboutDialog->setText( QCoreApplication::translate( "calamares-sidebar", "About", "@button" ) );
            aboutDialog->setToolTip(
                QCoreApplication::translate( "calamares-sidebar", "Show information about Calamares", "@tooltip" ) ); );
        extraButtons->addWidget( aboutDialog );
        aboutDialog->setFlat( true );
        aboutDialog->setCheckable( true );
        QObject::connect( aboutDialog, &QPushButton::clicked, debug, &Calamares::DebugWindowManager::about );
    }
    if ( debug && debug->enabled() )
    {
        QPushButton* debugWindowBtn = new QPushButton;
        debugWindowBtn->setObjectName( "debugButton" );
        debugWindowBtn->setIcon( Calamares::defaultPixmap(
            Calamares::Bugs, Calamares::Original, 2 * QSize( defaultFontHeight, defaultFontHeight ) ) );
        CALAMARES_RETRANSLATE_FOR(
            debugWindowBtn,
            debugWindowBtn->setText( QCoreApplication::translate( "calamares-sidebar", "Debug", "@button" ) );
            debugWindowBtn->setToolTip(
                QCoreApplication::translate( "calamares-sidebar", "Show debug information", "@tooltip" ) ); );
        extraButtons->addWidget( debugWindowBtn );
        debugWindowBtn->setFlat( true );
        debugWindowBtn->setCheckable( true );
        QObject::connect( debugWindowBtn, &QPushButton::clicked, debug, &Calamares::DebugWindowManager::show );
        QObject::connect(
            debug, &Calamares::DebugWindowManager::visibleChanged, debugWindowBtn, &QPushButton::setChecked );
    }

    Calamares::unmarginLayout( sideLayout );
    return sideBox;
}

static QWidget*
getWidgetNavigation( Calamares::DebugWindowManager*,
                     Calamares::ViewManager* viewManager,
                     QWidget* parent,
                     Qt::Orientation,
                     int )
{
    QWidget* navigation = new QWidget( parent );
    QBoxLayout* bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();

    // Create buttons and sets an initial icon; the icons may change
    {
        auto* back = new QPushButton(
            getButtonIcon( QStringLiteral( "go-previous" ) ),
            QCoreApplication::translate( CalamaresWindow::staticMetaObject.className(), "&Back", "@button" ),
            navigation );
        back->setObjectName( "view-button-back" );
        back->setEnabled( viewManager->backEnabled() );
        QObject::connect( back, &QPushButton::clicked, viewManager, &Calamares::ViewManager::back );
        QObject::connect( viewManager, &Calamares::ViewManager::backEnabledChanged, back, &QPushButton::setEnabled );
        QObject::connect( viewManager, &Calamares::ViewManager::backLabelChanged, back, &QPushButton::setText );
        QObject::connect(
            viewManager, &Calamares::ViewManager::backIconChanged, [ = ]( QString n ) { setButtonIcon( back, n ); } );
        QObject::connect(
            viewManager, &Calamares::ViewManager::backAndNextVisibleChanged, back, &QPushButton::setVisible );
        bottomLayout->addWidget( back );
    }
    {
        auto* next = new QPushButton(
            getButtonIcon( QStringLiteral( "go-next" ) ),
            QCoreApplication::translate( CalamaresWindow::staticMetaObject.className(), "&Next", "@button" ),
            navigation );
        next->setObjectName( "view-button-next" );
        next->setEnabled( viewManager->nextEnabled() );
        QObject::connect( next, &QPushButton::clicked, viewManager, &Calamares::ViewManager::next );
        QObject::connect( viewManager,
                          &Calamares::ViewManager::nextEnabledChanged,
                          next,
                          [ = ]( Calamares::ViewStep* step, bool enabled )
                          { setNextButtonEnabled( next, step, enabled ); } );
        QObject::connect( viewManager, &Calamares::ViewManager::nextLabelChanged, next, &QPushButton::setText );
        QObject::connect(
            viewManager, &Calamares::ViewManager::nextIconChanged, [ = ]( QString n ) { setButtonIcon( next, n ); } );
        QObject::connect(
            viewManager, &Calamares::ViewManager::backAndNextVisibleChanged, next, &QPushButton::setVisible );
        bottomLayout->addWidget( next );
    }
    bottomLayout->addSpacing( 12 );
    {
        auto* quit = new QPushButton(
            getButtonIcon( QStringLiteral( "dialog-cancel" ) ),
            QCoreApplication::translate( CalamaresWindow::staticMetaObject.className(), "&Cancel", "@button" ),
            navigation );
        quit->setObjectName( "view-button-cancel" );
        QObject::connect( quit, &QPushButton::clicked, viewManager, &Calamares::ViewManager::quit );
        QObject::connect( viewManager, &Calamares::ViewManager::quitEnabledChanged, quit, &QPushButton::setEnabled );
        QObject::connect( viewManager, &Calamares::ViewManager::quitLabelChanged, quit, &QPushButton::setText );
        QObject::connect(
            viewManager, &Calamares::ViewManager::quitIconChanged, [ = ]( QString n ) { setButtonIcon( quit, n ); } );
        QObject::connect( viewManager, &Calamares::ViewManager::quitTooltipChanged, quit, &QPushButton::setToolTip );
        QObject::connect( viewManager, &Calamares::ViewManager::quitVisibleChanged, quit, &QPushButton::setVisible );
        bottomLayout->addWidget( quit );
    }

    bottomLayout->setContentsMargins( 0, 0, 6, 6 );
    navigation->setLayout( bottomLayout );
    return navigation;
}

#ifdef WITH_QML

static inline void
setDimension( QQuickWidget* w, Qt::Orientation o, int desiredWidth )
{
    w->setSizePolicy( o == Qt::Orientation::Vertical ? QSizePolicy::MinimumExpanding : QSizePolicy::Expanding,
                      o == Qt::Orientation::Horizontal ? QSizePolicy::MinimumExpanding : QSizePolicy::Expanding );
    if ( o == Qt::Orientation::Vertical )
    {
        w->setFixedWidth( desiredWidth );
    }
    else
    {
        // If the QML itself sets a height, use that, otherwise go to 48 pixels
        // which seems to match what the widget navigation would use for height
        // (with *my* specific screen, style, etc. so YMMV).
        //
        // Bound between (16, 64) with a default of 48.
        qreal minimumHeight = qBound( qreal( 16 ), w->rootObject() ? w->rootObject()->height() : 48, qreal( 64 ) );
        w->setMinimumHeight( int( minimumHeight ) );
        w->setFixedHeight( int( minimumHeight ) );
    }
    w->setResizeMode( QQuickWidget::SizeRootObjectToView );
}

static QWidget*
getQmlSidebar( Calamares::DebugWindowManager* debug,
               Calamares::ViewManager*,
               QWidget* parent,
               Qt::Orientation o,
               int desiredWidth )
{
    Calamares::registerQmlModels();
    QQuickWidget* w = new QQuickWidget( parent );
    if ( debug )
    {
        w->engine()->rootContext()->setContextProperty( "debug", debug );
    }

    w->setSource(
        QUrl( Calamares::searchQmlFile( Calamares::QmlSearch::Both, QStringLiteral( "calamares-sidebar" ) ) ) );
    setDimension( w, o, desiredWidth );
    return w;
}

static QWidget*
getQmlNavigation( Calamares::DebugWindowManager* debug,
                  Calamares::ViewManager*,
                  QWidget* parent,
                  Qt::Orientation o,
                  int desiredWidth )
{
    Calamares::registerQmlModels();
    QQuickWidget* w = new QQuickWidget( parent );
    if ( debug )
    {
        w->engine()->rootContext()->setContextProperty( "debug", debug );
    }
    w->setSource(
        QUrl( Calamares::searchQmlFile( Calamares::QmlSearch::Both, QStringLiteral( "calamares-navigation" ) ) ) );
    setDimension( w, o, desiredWidth );
    return w;
}
#else
// Bogus to keep the linker happy
//
// Calls to flavoredWidget() still refer to these *names*
// even if they are subsequently not used.
static QWidget*
getQmlSidebar( Calamares::DebugWindowManager*,
               Calamares::ViewManager*,
               QWidget* parent,
               Qt::Orientation,
               int desiredWidth )
{
    return nullptr;
}
static QWidget*
getQmlNavigation( Calamares::DebugWindowManager*,
                  Calamares::ViewManager*,
                  QWidget* parent,
                  Qt::Orientation,
                  int desiredWidth )
{
    return nullptr;
}
#endif

/**@brief Picks one of two methods to call
 *
 * Calls method (member function) @p widget or @p qml with arguments @p a
 * on the given window, based on the flavor.
 */
template < typename widgetMaker, typename... args >
QWidget*
flavoredWidget( Calamares::Branding::PanelFlavor flavor,
                Qt::Orientation o,
                Calamares::DebugWindowManager* w,
                QWidget* parent,
                widgetMaker widget,
                widgetMaker qml,  // Only if WITH_QML is on
                args... a )
{
#ifndef WITH_QML
    Q_UNUSED( qml )
#endif
    auto* viewManager = Calamares::ViewManager::instance();
    switch ( flavor )
    {
    case Calamares::Branding::PanelFlavor::Widget:
        return widget( w, viewManager, parent, o, a... );
#ifdef WITH_QML
    case Calamares::Branding::PanelFlavor::Qml:
        return qml( w, viewManager, parent, o, a... );
#endif
    case Calamares::Branding::PanelFlavor::None:
        return nullptr;
    }
    __builtin_unreachable();
}

/** @brief Adds widgets to @p layout if they belong on this @p side
 */
static inline void
insertIf( QBoxLayout* layout,
          Calamares::Branding::PanelSide side,
          QWidget* first,
          Calamares::Branding::PanelSide firstSide )
{
    if ( first && side == firstSide )
    {
        layout->addWidget( first );
    }
}

CalamaresWindow::CalamaresWindow( QWidget* parent )
    : QWidget( parent )
    , m_debugManager( new Calamares::DebugWindowManager( this ) )
    , m_viewManager( nullptr )
{
    installEventFilter( Calamares::Retranslator::instance() );

    // If we can never cancel, don't show the window-close button
    if ( Calamares::Settings::instance()->disableCancel() )
    {
        setWindowFlag( Qt::WindowCloseButtonHint, false );
    }

    // %1 is the distribution name
    CALAMARES_RETRANSLATE( const auto* branding = Calamares::Branding::instance();
                           setWindowTitle( Calamares::Settings::instance()->isSetupMode()
                                               ? tr( "%1 Setup Program" ).arg( branding->productName() )
                                               : tr( "%1 Installer" ).arg( branding->productName() ) ); );

    const Calamares::Branding* const branding = Calamares::Branding::instance();
    using ImageEntry = Calamares::Branding::ImageEntry;

    // Branding "fullscreen" expansion selects the deepin-installer-like
    // full-screen shell instead of the classic sidebar window.
    m_fullscreenMode = branding->windowMaximize();
    if ( m_fullscreenMode )
    {
        initFullscreenUI();
        return;
    }

    using Calamares::windowMinimumHeight;
    using Calamares::windowMinimumWidth;
    using Calamares::windowPreferredHeight;
    using Calamares::windowPreferredWidth;

    using PanelSide = Calamares::Branding::PanelSide;

    // Needs to match what's checked in DebugWindow
    this->setObjectName( "mainApp" );

    QSize availableSize = desktopSize( this );
    QSize minimumSize( qBound( windowMinimumWidth, availableSize.width(), windowPreferredWidth ),
                       qBound( windowMinimumHeight, availableSize.height(), windowPreferredHeight ) );
    setMinimumSize( minimumSize );

    cDebug() << "Available desktop" << availableSize << "minimum size" << minimumSize;

    auto brandingSizes = branding->windowSize();

    int w = qBound( minimumSize.width(), windowDimensionToPixels( brandingSizes.first ), availableSize.width() );
    int h = qBound( minimumSize.height(), windowDimensionToPixels( brandingSizes.second ), availableSize.height() );

    cDebug() << Logger::SubEntry << "Proposed window size:" << w << h;
    resize( w, h );

    QWidget* baseWidget = this;
    if ( !( branding->imagePath( ImageEntry::ProductWallpaper ).isEmpty() ) )
    {
        QWidget* label = new QWidget( this );
        QVBoxLayout* l = new QVBoxLayout;
        Calamares::unmarginLayout( l );
        l->addWidget( label );
        setLayout( l );
        label->setObjectName( "backgroundWidget" );
        label->setStyleSheet(
            QStringLiteral( "#backgroundWidget { background-image: url(%1); background-repeat: repeat-xy; }" )
                .arg( branding->imagePath( ImageEntry::ProductWallpaper ) ) );

        baseWidget = label;
    }

    m_viewManager = Calamares::ViewManager::instance( baseWidget );
    if ( branding->windowExpands() )
    {
        connect( m_viewManager, &Calamares::ViewManager::ensureSize, this, &CalamaresWindow::ensureSize );
    }
    // NOTE: Although the ViewManager has a signal cancelEnabled() that
    //       signals when the state of the cancel button changes (in
    //       particular, to disable cancel during the exec phase),
    //       we don't connect to it here. Changing the window flag
    //       for the close button causes uncomfortable window flashing
    //       and requires an extra show() (at least with KWin/X11) which
    //       is too annoying. Instead, leave it up to ignoring-the-quit-
    //       event, which is also the ViewManager's responsibility.

    QBoxLayout* mainLayout = new QHBoxLayout;
    QBoxLayout* contentsLayout = new QVBoxLayout;
    contentsLayout->setSpacing( 0 );

    QWidget* sideBox
        = flavoredWidget( branding->sidebarFlavor(),
                          ::orientation( branding->sidebarSide() ),
                          m_debugManager,
                          baseWidget,
                          ::getWidgetSidebar,
                          ::getQmlSidebar,
                          qBound( 100, Calamares::defaultFontHeight() * 12, w < windowPreferredWidth ? 100 : 190 ) );
    QWidget* navigation = flavoredWidget( branding->navigationFlavor(),
                                          ::orientation( branding->navigationSide() ),
                                          m_debugManager,
                                          baseWidget,
                                          ::getWidgetNavigation,
                                          ::getQmlNavigation,
                                          64 );

    // Build up the contentsLayout (a VBox) top-to-bottom
    // .. note that the bottom is mirrored wrt. the top
    insertIf( contentsLayout, PanelSide::Top, sideBox, branding->sidebarSide() );
    insertIf( contentsLayout, PanelSide::Top, navigation, branding->navigationSide() );
    contentsLayout->addWidget( m_viewManager->centralWidget() );
    insertIf( contentsLayout, PanelSide::Bottom, navigation, branding->navigationSide() );
    insertIf( contentsLayout, PanelSide::Bottom, sideBox, branding->sidebarSide() );

    // .. and then the mainLayout left-to-right
    insertIf( mainLayout, PanelSide::Left, sideBox, branding->sidebarSide() );
    insertIf( mainLayout, PanelSide::Left, navigation, branding->navigationSide() );
    mainLayout->addLayout( contentsLayout );
    insertIf( mainLayout, PanelSide::Right, navigation, branding->navigationSide() );
    insertIf( mainLayout, PanelSide::Right, sideBox, branding->sidebarSide() );

    // layout->count() returns number of things in it; above we have put
    // at **least** the central widget, which comes from the view manager,
    // both vertically and horizontally -- so if there's a panel along
    // either axis, the count in that axis will be > 1.
    m_viewManager->setPanelSides(
        ( contentsLayout->count() > 1 ? Qt::Orientations( Qt::Horizontal ) : Qt::Orientations() )
        | ( mainLayout->count() > 1 ? Qt::Orientations( Qt::Vertical ) : Qt::Orientations() ) );

    Calamares::unmarginLayout( mainLayout );
    Calamares::unmarginLayout( contentsLayout );
    baseWidget->setLayout( mainLayout );
    setStyleSheet( Calamares::Branding::instance()->stylesheet() );
}

void
CalamaresWindow::ensureSize( QSize size )
{
    auto mainGeometry = this->geometry();
    QSize availableSize = desktopSize( this );

    // We only care about vertical sizes that are big enough
    int embiggenment = qMax( 0, size.height() - m_viewManager->centralWidget()->size().height() );
    if ( embiggenment < 6 )
    {
        return;
    }

    auto h = qBound( 0, mainGeometry.height() + embiggenment, availableSize.height() );
    auto w = this->size().width();

    resize( w, h );
}

void
CalamaresWindow::closeEvent( QCloseEvent* event )
{
    if ( m_viewManager )
    {
        m_viewManager->quit();
        // If it didn't actually exit, eat the event to ignore close
        event->ignore();
    }
    else
    {
        event->accept();
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
        QApplication::quit();
#else
        QApplication::exit( EXIT_SUCCESS );
#endif
    }
}

void
CalamaresWindow::resizeEvent( QResizeEvent* event )
{
    if ( m_fullscreenMode )
    {
        updateBackground();
    }
    QWidget::resizeEvent( event );
}

/** @brief A 40x40 borderless corner button, like deepin-installer's close button.
 *
 * The close button gets its image from the deepin style sheet; the other
 * corner buttons carry a Calamares icon and are painted translucent.
 */
static QPushButton*
makeCornerButton( QWidget* parent, const QString& objectName )
{
    QPushButton* button = new QPushButton( parent );
    button->setObjectName( objectName );
    button->setFlat( true );
    button->setFocusPolicy( Qt::TabFocus );
    button->setFixedSize( 40, 40 );
    return button;
}

void
CalamaresWindow::initFullscreenUI()
{
    // Needs to match what's checked in DebugWindow
    this->setObjectName( "mainApp" );

    // deepin-installer runs as a frameless full-screen window
    this->setWindowFlags( Qt::FramelessWindowHint );

    // Dark-wallpaper friendly palette, so widgets and custom-drawn
    // delegates (e.g. the welcome requirements list) blend with the
    // wallpaper instead of painting bright solid blocks on top of it.
    QPalette pal = QApplication::palette();
    const QColor translucentWhite = QColor( 255, 255, 255, 30 );
    pal.setColor( QPalette::Window, translucentWhite );
    pal.setColor( QPalette::Base, QColor( 255, 255, 255, 20 ) );
    pal.setColor( QPalette::AlternateBase, QColor( 255, 255, 255, 15 ) );
    pal.setColor( QPalette::Button, translucentWhite );
    pal.setColor( QPalette::WindowText, Qt::white );
    pal.setColor( QPalette::Text, Qt::white );
    pal.setColor( QPalette::ButtonText, Qt::white );
    pal.setColor( QPalette::Highlight, QColor( 255, 255, 255, 60 ) );
    pal.setColor( QPalette::HighlightedText, Qt::white );
    pal.setColor( QPalette::ToolTipBase, QColor( 60, 60, 60, 230 ) );
    pal.setColor( QPalette::ToolTipText, Qt::white );
    QApplication::setPalette( pal );

    // Wallpaper background; created first so it is painted behind everything
    m_background = new QLabel( this );
    m_background->setObjectName( "deepinBackground" );
    m_background->setScaledContents( false );

    m_viewManager = Calamares::ViewManager::instance( this );
    // No side panels in the deepin shell; pages span the whole window
    m_viewManager->setPanelSides( {} );

    // Top-right corner buttons (about, debug, close), like deepin-installer's
    // close_button_ in MainWindow::initUI()
    m_closeButton = makeCornerButton( this, "deepinCloseButton" );
    QObject::connect( m_closeButton, &QPushButton::clicked, m_viewManager, &Calamares::ViewManager::quit );

    QHBoxLayout* cornerLayout = new QHBoxLayout();
    cornerLayout->setContentsMargins( 0, 0, 0, 0 );
    cornerLayout->setSpacing( 0 );
    cornerLayout->addStretch();
    if ( m_debugManager->enabled() )
    {
        m_aboutButton = makeCornerButton( this, "deepinCornerButton" );
        m_aboutButton->setIcon(
            Calamares::defaultPixmap( Calamares::Information, Calamares::Original, QSize( 22, 22 ) ) );
        CALAMARES_RETRANSLATE_FOR(
            m_aboutButton,
            m_aboutButton->setToolTip( QCoreApplication::translate( "calamares-sidebar", "About", "@button" ) ); );
        QObject::connect( m_aboutButton, &QPushButton::clicked, m_debugManager, &Calamares::DebugWindowManager::about );
        cornerLayout->addWidget( m_aboutButton );

        m_debugButton = makeCornerButton( this, "deepinCornerButton" );
        m_debugButton->setIcon( Calamares::defaultPixmap( Calamares::Bugs, Calamares::Original, QSize( 22, 22 ) ) );
        CALAMARES_RETRANSLATE_FOR(
            m_debugButton,
            m_debugButton->setToolTip( QCoreApplication::translate( "calamares-sidebar", "Debug", "@button" ) ); );
        QObject::connect( m_debugButton, &QPushButton::clicked, m_debugManager, &Calamares::DebugWindowManager::show );
        QObject::connect(
            m_debugManager, &Calamares::DebugWindowManager::visibleChanged, m_debugButton, &QPushButton::setChecked );
        cornerLayout->addWidget( m_debugButton );
    }
    cornerLayout->addWidget( m_closeButton );
    // Add 4px at top and right margin, like deepin-installer's close_button_wrapper
    QFrame* cornerWrapper = new QFrame( this );
    cornerWrapper->setContentsMargins( 0, 4, 4, 0 );
    cornerWrapper->setLayout( cornerLayout );

    // Large page title, like deepin-installer's TitleLabel on each page
    m_titleLabel = new QLabel( this );
    m_titleLabel->setObjectName( "deepinTitle" );
    m_titleLabel->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    m_titleLabel->setFixedHeight( 56 );

    // Central widget holds the ViewManager pages
    QWidget* centralWidget = m_viewManager->centralWidget();
    centralWidget->setParent( this );

    // Navigation buttons (Back / Next), styled like deepin-installer 15's
    // NavButton: narrow buttons stacked vertically and centered. The
    // mnemonics ("&Next" -> "Next") are stripped from the labels.
    const QSize navButtonSize( 200, 36 );
    m_backButton = new QPushButton( this );
    m_backButton->setObjectName( "deepinNavButton" );
    m_backButton->setFixedSize( navButtonSize );
    m_backButton->setEnabled( m_viewManager->backEnabled() );
    m_backButton->setText( stripButtonMnemonic( m_viewManager->backLabel() ) );
    QObject::connect( m_backButton, &QPushButton::clicked, m_viewManager, &Calamares::ViewManager::back );
    QObject::connect( m_viewManager, &Calamares::ViewManager::backEnabledChanged, m_backButton, &QPushButton::setEnabled );
    QObject::connect( m_viewManager, &Calamares::ViewManager::backLabelChanged, m_backButton, [ = ]( QString t )
                      { m_backButton->setText( stripButtonMnemonic( t ) ); } );

    m_nextButton = new QPushButton( this );
    m_nextButton->setObjectName( "deepinNavButton" );
    m_nextButton->setFixedSize( navButtonSize );
    m_nextButton->setEnabled( m_viewManager->nextEnabled() );
    m_nextButton->setText( stripButtonMnemonic( m_viewManager->nextLabel() ) );
    QObject::connect( m_nextButton, &QPushButton::clicked, m_viewManager, &Calamares::ViewManager::next );
    QObject::connect( m_viewManager,
                      &Calamares::ViewManager::nextEnabledChanged,
                      m_nextButton,
                      [ = ]( Calamares::ViewStep* step, bool enabled )
                      { setNextButtonEnabled( m_nextButton, step, enabled ); } );
    QObject::connect( m_viewManager, &Calamares::ViewManager::nextLabelChanged, m_nextButton, [ = ]( QString t )
                      { m_nextButton->setText( stripButtonMnemonic( t ) ); } );

    // Stacked vertically and centered, like deepin-installer 15
    QVBoxLayout* navLayout = new QVBoxLayout();
    navLayout->setContentsMargins( 0, 0, 0, 0 );
    navLayout->setSpacing( 15 );
    navLayout->addWidget( m_backButton, 0, Qt::AlignHCenter );
    navLayout->addWidget( m_nextButton, 0, Qt::AlignHCenter );
    m_navRow = new QWidget( this );
    m_navRow->setLayout( navLayout );

    // Page-indicator dots at the bottom of the window, like deepin-installer's
    // page_indicator_ in MainWindow::initUI()
    m_pageIndicator = new PageIndicator( 0, this );
    m_indicatorWrapper = new QFrame( this );
    m_indicatorWrapper->setFixedHeight( 48 );
    QHBoxLayout* indicatorLayout = new QHBoxLayout();
    indicatorLayout->setContentsMargins( 0, 0, 0, 0 );
    indicatorLayout->addWidget( m_pageIndicator );
    m_indicatorWrapper->setLayout( indicatorLayout );

    // Content is inset from the window edges, like deepin-installer 15
    QHBoxLayout* centralLayout = new QHBoxLayout();
    centralLayout->setContentsMargins( 40, 0, 40, 0 );
    centralLayout->addWidget( centralWidget );

    QVBoxLayout* vboxLayout = new QVBoxLayout();
    vboxLayout->setContentsMargins( 0, 0, 0, 0 );
    vboxLayout->setSpacing( 0 );
    vboxLayout->addWidget( cornerWrapper );
    vboxLayout->addSpacing( 10 );
    vboxLayout->addWidget( m_titleLabel );
    vboxLayout->addLayout( centralLayout, 1 );
    vboxLayout->addWidget( m_navRow );
    vboxLayout->addWidget( m_indicatorWrapper );
    vboxLayout->addSpacing( 32 );

    this->setLayout( vboxLayout );
    this->setContentsMargins( 0, 0, 0, 0 );

    setStyleSheet( Calamares::Branding::instance()->stylesheet() );

    // Refresh title / indicator / button visibility when anything changes
    QObject::connect(
        m_viewManager, &Calamares::ViewManager::currentStepChanged, this, &CalamaresWindow::updateFullscreenPage );
    QObject::connect(
        m_viewManager, &Calamares::ViewManager::backAndNextVisibleChanged, this, &CalamaresWindow::updateFullscreenPage );
    QObject::connect(
        m_viewManager, &Calamares::ViewManager::rowsInserted, this, &CalamaresWindow::updateFullscreenPage );
    // Hide the close button while installation is running (cancel disabled),
    // and hide it entirely when cancel is disabled; deepin-installer does the same.
    QObject::connect( m_viewManager, &Calamares::ViewManager::cancelEnabled, m_closeButton, &QWidget::setVisible );
    QObject::connect( m_viewManager, &Calamares::ViewManager::quitVisibleChanged, m_closeButton, &QWidget::setVisible );
    QObject::connect( m_viewManager,
                      &Calamares::ViewManager::quitTooltipChanged,
                      m_closeButton,
                      &QWidget::setToolTip );

    updateFullscreenPage();
}

void
CalamaresWindow::updateFullscreenPage()
{
    if ( !m_viewManager || !m_pageIndicator )
    {
        return;
    }

    const int steps = m_viewManager->viewSteps().count();
    const int current = m_viewManager->currentStepIndex();

    m_pageIndicator->updatePages( steps );
    m_pageIndicator->setCurrentPage( current );

    // The step's name as a large centered title, deepin-installer style
    const QModelIndex index( m_viewManager->index( current, 0 ) );
    m_titleLabel->setText( m_viewManager->data( index, Qt::DisplayRole ).toString() );

    // The very last page (all done) has no navigation and no dots,
    // like deepin-installer's install-success page
    const bool last = ( current >= steps - 1 );
    m_indicatorWrapper->setVisible( !last );
    m_navRow->setVisible( !last && m_viewManager->backAndNextVisible() );
}

void
CalamaresWindow::updateBackground()
{
    if ( !m_background )
    {
        return;
    }

    QString imagePath = Calamares::Branding::instance()->imagePath( Calamares::Branding::ImageEntry::ProductWallpaper );
    if ( !imagePath.isEmpty() )
    {
        // Branding image paths are relative to the branding component directory
        imagePath = QDir( Calamares::Branding::instance()->componentDirectory() ).absoluteFilePath( imagePath );
    }
    if ( imagePath.isEmpty() || !QFileInfo::exists( imagePath ) )
    {
        imagePath = QStringLiteral( ":/deepin/default_wallpaper.jpg" );
    }
    // Other platforms may have performance issues, deepin scales
    // KeepAspectRatioByExpanding so the wallpaper always covers the window.
    const QPixmap pixmap = QPixmap( imagePath ).scaled( size(), Qt::KeepAspectRatioByExpanding );
    m_background->setPixmap( pixmap );
    m_background->setFixedSize( size() );
}
