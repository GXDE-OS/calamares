/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2015 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "WelcomePage.h"

#include "checker/CheckerContainer.h"
#include "ui_WelcomePage.h"

#include "Branding.h"
#include "CalamaresAbout.h"
#include "CalamaresVersion.h"
#include "Config.h"
#include "Settings.h"
#include "ViewManager.h"

#include "modulesystem/ModuleManager.h"
#include "modulesystem/RequirementsModel.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/NamedEnum.h"
#include "utils/Retranslator.h"

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QDesktopServices>
#include <QFocusEvent>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QScreen>

WelcomePage::WelcomePage( Config* config, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::WelcomePage )
    , m_checkingWidget( new CheckerContainer( config, this ) )
    , m_languages( nullptr )
    , m_conf( config )
{
    using Branding = Calamares::Branding;

    const int defaultFontHeight = Calamares::defaultFontHeight();
    ui->setupUi( this );

    // insert system-check widget below welcome text
    const int welcome_text_idx = ui->verticalLayout->indexOf( ui->mainText );
    ui->verticalLayout->insertWidget( welcome_text_idx + 1, m_checkingWidget );

    // insert optional logo banner image above welcome text
    QString bannerPath = Branding::instance()->imagePath( Branding::ProductBanner );
    if ( !bannerPath.isEmpty() )
    {
        // If the name is not empty, the file exists -- Branding checks that at startup
        QPixmap bannerPixmap = QPixmap( bannerPath );
        if ( !bannerPixmap.isNull() )
        {
            QLabel* bannerLabel = new QLabel;
            bannerLabel->setPixmap( bannerPixmap );
            bannerLabel->setMinimumHeight( 64 );
            bannerLabel->setAlignment( Qt::AlignCenter );
            ui->aboveTextSpacer->changeSize( 20, defaultFontHeight );  // Shrink it down
            ui->aboveTextSpacer->invalidate();
            ui->verticalLayout->insertSpacing( welcome_text_idx, defaultFontHeight );
            ui->verticalLayout->insertWidget( welcome_text_idx, bannerLabel );
        }
    }

    initLanguages();

    CALAMARES_RETRANSLATE_SLOT( &WelcomePage::retranslate );

    connect( Calamares::ModuleManager::instance(),
             &Calamares::ModuleManager::requirementsComplete,
             m_checkingWidget,
             &CheckerContainer::requirementsComplete );
    connect( Calamares::ModuleManager::instance()->requirementsModel(),
             &Calamares::RequirementsModel::progressMessageChanged,
             m_checkingWidget,
             &CheckerContainer::requirementsProgress );
}

void
WelcomePage::init()
{
    //setup the url buttons
    setupButton( WelcomePage::Button::Support, m_conf->supportUrl() );
    setupButton( WelcomePage::Button::KnownIssues, m_conf->knownIssuesUrl() );
    setupButton( WelcomePage::Button::ReleaseNotes, m_conf->releaseNotesUrl() );
    setupButton( WelcomePage::Button::Donate, m_conf->donateUrl() );

    //language icon
    auto icon = Calamares::Branding::instance()->image( m_conf->languageIcon(), QSize( 48, 48 ) );
    if ( !icon.isNull() )
    {
        setLanguageIcon( icon );
    }
}

void
WelcomePage::initLanguages()
{
    // deepin-installer style: a large, centered language list instead of a
    // combo box, with the product logo on top of the page.
    if ( !m_languageList )
    {
        m_languageList = new QListView( this );
        m_languageList->setObjectName( "languageList" );
        m_languageList->setSelectionMode( QAbstractItemView::SingleSelection );
        m_languageList->setEditTriggers( QAbstractItemView::NoEditTriggers );
        m_languageList->setUniformItemSizes( true );
        m_languageList->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
        m_languageList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

        // The list covers most of the screen, like deepin-installer
        const int listWidth = qBound( 480, QApplication::primaryScreen()->size().width() * 3 / 5, 1200 );
        const int listHeight = qBound( 300, QApplication::primaryScreen()->size().height() * 2 / 5, 620 );
        m_languageList->setMinimumWidth( listWidth );
        m_languageList->setMaximumWidth( listWidth );
        m_languageList->setMinimumHeight( listHeight );

        // Product logo on top of the page
        const QPixmap logo
            = Calamares::Branding::instance()->image( Calamares::Branding::ProductLogo, QSize( 128, 128 ) );
        if ( !logo.isNull() )
        {
            auto* logoLabel = new QLabel( this );
            logoLabel->setObjectName( "welcomeLogo" );
            logoLabel->setPixmap( logo );
            logoLabel->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
            logoLabel->setFixedHeight( 128 );
            const int logoIdx = ui->verticalLayout->indexOf( ui->aboveTextSpacer );
            ui->verticalLayout->insertWidget( logoIdx < 0 ? 0 : logoIdx, logoLabel, 0, Qt::AlignHCenter );
        }

        // Replace the combo-box row with the list
        const int comboRowIdx = ui->verticalLayout->indexOf( ui->horizontalLayout_3 );
        const int listStretch = 1;
        if ( comboRowIdx >= 0 )
        {
            ui->verticalLayout->insertWidget( comboRowIdx, m_languageList, listStretch );
        }
        else
        {
            ui->verticalLayout->addWidget( m_languageList, listStretch );
        }
        ui->verticalLayout->removeItem( ui->horizontalLayout_3 );
        // The orphaned combo-box widgets would show up at the top-left
        ui->languageWidget->hide();
        ui->languageIcon->hide();
        // The deepin-installer page has no web-link buttons; hide the row
        // so it cannot overlap the language list.
        ui->verticalLayout->removeItem( ui->horizontalLayout_2 );
        for ( auto* b : { ui->donateButton, ui->supportButton, ui->knownIssuesButton, ui->releaseNotesButton } )
        {
            b->hide();
        }
        // The list is centered within the page (its width is capped above)
        ui->verticalLayout->setAlignment( m_languageList, Qt::AlignHCenter );
        ui->verticalLayout->setStretchFactor( m_languageList, 1 );
    }

    // Fill the list of translations
    m_languageList->setModel( m_conf->languagesModel() );
    m_languageList->setItemDelegate( new LocaleTwoColumnDelegate( m_languageList ) );

    m_languageList->setCurrentIndex( m_conf->languagesModel()->index( m_conf->localeIndex(), 0 ) );

    connect( m_languageList->selectionModel(),
             &QItemSelectionModel::currentRowChanged,
             m_conf,
             [ this ]( const QModelIndex& current, const QModelIndex& )
             { m_conf->setLocaleIndex( current.row() ); } );
}

void
WelcomePage::setupButton( Button role, const QString& url )
{
    QPushButton* button = nullptr;
    Calamares::ImageType icon = Calamares::Information;

    switch ( role )
    {
    case Button::Donate:
        button = ui->donateButton;
        icon = Calamares::Donate;
        break;
    case Button::KnownIssues:
        button = ui->knownIssuesButton;
        icon = Calamares::Bugs;
        break;
    case Button::ReleaseNotes:
        button = ui->releaseNotesButton;
        icon = Calamares::Release;
        break;
    case Button::Support:
        button = ui->supportButton;
        icon = Calamares::Help;
        break;
    }
    if ( !button )
    {
        cWarning() << "Unknown button role" << smash( role );
        return;
    }

    if ( url.isEmpty() )
    {
        button->hide();
        return;
    }

    QUrl u( url );
    if ( u.isValid() )
    {
        auto size = 2 * QSize( Calamares::defaultFontHeight(), Calamares::defaultFontHeight() );
        button->setIcon( Calamares::defaultPixmap( icon, Calamares::Original, size ) );
        connect( button, &QPushButton::clicked, [ u ]() { QDesktopServices::openUrl( u ); } );
    }
    else
    {
        cWarning() << "Welcome button" << smash( role ) << "URL" << url << "is invalid.";
        button->hide();
    }
}

void
WelcomePage::focusInEvent( QFocusEvent* e )
{
    if ( ui->languageWidget )
    {
        ui->languageWidget->setFocus();
    }
    e->accept();
}

bool
WelcomePage::verdict() const
{
    return m_checkingWidget->verdict();
}

void
WelcomePage::externallySelectedLanguage( int row )
{
    if ( m_languageList && ( row >= 0 ) && ( row < m_languageList->model()->rowCount() ) )
    {
        m_languageList->setCurrentIndex( m_languageList->model()->index( row, 0 ) );
    }
}

void
WelcomePage::setLanguageIcon( QPixmap i )
{
    ui->languageIcon->setPixmap( i );
}

void
WelcomePage::retranslate()
{
    const QString message = m_conf->genericWelcomeMessage();

    ui->mainText->setText( message.arg( Calamares::Branding::instance()->versionedName() ) );
    ui->retranslateUi( this );
    ui->supportButton->setText(
        tr( "%1 Support", "@action" ).arg( Calamares::Branding::instance()->shortProductName() ) );
}

void
LocaleTwoColumnDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QStyledItemDelegate::paint( painter, option, index );
    option.widget->style()->drawItemText(
        painter,
        option.rect,
        Qt::AlignRight | Qt::AlignVCenter,
        option.palette,
        false,
        index.data( Calamares::Locale::TranslationsModel::EnglishLabelRole ).toString() );
}
