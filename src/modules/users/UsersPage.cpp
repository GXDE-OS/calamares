/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-FileCopyrightText: 2020 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Portions from the Manjaro Installation Framework
 *   by Roland Singer <roland@manjaro.org>
 *   Copyright (C) 2007 Free Software Foundation, Inc.
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "UsersPage.h"

#include "Config.h"
#include "Branding.h"
#include "ui_page_usersetup.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "Settings.h"
#include "compat/CheckBox.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "utils/String.h"

#include <QAction>
#include <QBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QStyle>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QBoxLayout>
#include <QDir>
#include <QLayoutItem>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QSizePolicy>
#include <QPushButton>
#include <QPainterPath>
#include <QToolButton>

UsersPage::UsersPage( Config* config, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::Page_UserSetup )
    , m_config( config )
{
    ui->setupUi( this );

    // ----- deepin-installer style form -----
    // Rebuild the .ui layout as a centered single column: every label is
    // centered above its line edit, all edits share one column width, the
    // password and its confirmation stack vertically, and an avatar button
    // sits on top of the form.
    const int fieldWidth = qBound( 300, Calamares::defaultFontHeight() * 22, 420 );

    m_avatarButton = new QToolButton( this );
    m_avatarButton->setObjectName( "userAvatarButton" );
    m_avatarButton->setFixedSize( 88, 88 );
    m_avatarButton->setIconSize( QSize( 84, 84 ) );
    m_avatarButton->setCursor( Qt::PointingHandCursor );
    updateAvatarIcon();
    connect( m_avatarButton, &QToolButton::clicked, this, &UsersPage::pickAvatar );
    connect( config, &Config::userAvatarChanged, this, &UsersPage::updateAvatarIcon );

    // Detach all items from the .ui layout; the widgets are re-used below.
    // The Active Directory group is kept as a whole.
    QLayout* adLayout = nullptr;
    while ( ui->verticalLayout_12->count() > 0 )
    {
        QLayoutItem* item = ui->verticalLayout_12->takeAt( 0 );
        if ( item->layout() == ui->verticalLayout )
        {
            // Keep the Active Directory group for re-use (deleting the item
            // would delete the layout object itself)
            adLayout = ui->verticalLayout;
            continue;
        }
        if ( auto* row = item->layout() )
        {
            while ( row->count() > 0 )
            {
                delete row->takeAt( 0 );  // widgets stay alive, re-added below
            }
        }
        delete item;
    }

    // A field = a single line edit, centered, with a leading icon that
    // identifies it (user / computer / lock), a trailing status icon
    // (tick / cross) and a colored glow around the border when the
    // value is ok or erroneous, deepin-installer style.
    auto addField = [ this, fieldWidth ]( QLineEdit* edit, const QString& leadingIcon )
    {
        edit->setMinimumHeight( 36 );
        edit->setMinimumWidth( fieldWidth );
        edit->setMaximumWidth( fieldWidth );

        // Leading icon is built into the installer (never read from the
        // system theme), a white monochrome glyph
        QIcon icon( QStringLiteral( ":/images/%1.svg" ).arg( leadingIcon ) );
        if ( !icon.isNull() )
        {
            edit->addAction( icon, QLineEdit::LeadingPosition );
        }
        QAction* statusAction = edit->addAction( QIcon(), QLineEdit::TrailingPosition );
        statusAction->setVisible( false );

        auto* glow = new QGraphicsDropShadowEffect( edit );
        glow->setOffset( 0, 0 );
        glow->setBlurRadius( 14 );
        glow->setColor( QColor( 0, 0, 0, 0 ) );
        edit->setGraphicsEffect( glow );

        // Translucent error bubble under the edit, hidden by default
        auto* tip = new QLabel( edit->parentWidget() );
        tip->setObjectName( "fieldErrorTip" );
        tip->setAlignment( Qt::AlignHCenter );
        tip->setWordWrap( true );
        tip->setMaximumWidth( fieldWidth + 60 );
        tip->hide();

        m_fields.insert( edit, { glow, statusAction, tip } );

        auto* editRow = new QHBoxLayout;
        editRow->addStretch();
        editRow->addWidget( edit );
        editRow->addStretch();
        ui->verticalLayout_12->addLayout( editRow );
        ui->verticalLayout_12->addWidget( tip, 0, Qt::AlignHCenter );
    };

    ui->verticalLayout_12->setSpacing( 10 );
    ui->verticalLayout_12->addStretch( 1 );
    ui->verticalLayout_12->addWidget( m_avatarButton, 0, Qt::AlignHCenter );
    ui->verticalLayout_12->addSpacing( 10 );

    // deepin-installer merges the full name and the login name into a
    // single field: hide the separate full-name field and mirror the
    // login name into the full name. Labels are hidden as well; the
    // line-edit placeholders and leading icons carry the meaning.
    ui->labelWhatIsYourName->hide();
    ui->textBoxFullName->hide();
    ui->labelFullName->hide();
    ui->labelFullNameError->hide();
    ui->username_label_2->hide();
    ui->hostnameLabel->hide();
    ui->password_label_2->hide();
    ui->labelChooseRootPassword->hide();
    addField( ui->textBoxLoginName, QStringLiteral( "user" ) );
    addField( ui->textBoxHostname, QStringLiteral( "computer" ) );
    addField( ui->textBoxUserPassword, QStringLiteral( "system-lock-screen" ) );
    addField( ui->textBoxUserVerifiedPassword, QStringLiteral( "system-lock-screen" ) );
    addField( ui->textBoxRootPassword, QStringLiteral( "system-lock-screen" ) );
    addField( ui->textBoxVerifiedRootPassword, QStringLiteral( "system-lock-screen" ) );

    for ( auto* w : { ui->checkBoxRequireStrongPassword, ui->checkBoxDoAutoLogin, ui->checkBoxReusePassword } )
    {
        ui->verticalLayout_12->addWidget( w, 0, Qt::AlignHCenter );
    }
    if ( adLayout )
    {
        // Wrap the Active Directory group in a centered container
        auto* adContainer = new QWidget( this );
        auto* adOuter = new QVBoxLayout( adContainer );
        adOuter->setContentsMargins( 0, 0, 0, 0 );
        adOuter->addLayout( adLayout );
        ui->verticalLayout_12->addWidget( adContainer, 0, Qt::AlignHCenter );
    }
    ui->verticalLayout_12->addStretch( 1 );

    // Connect signals and slots
    ui->textBoxUserPassword->setText( config->userPassword() );
    connect( ui->textBoxUserPassword, &QLineEdit::textChanged, config, &Config::setUserPassword );
    connect( config, &Config::userPasswordChanged, ui->textBoxUserPassword, &QLineEdit::setText );
    ui->textBoxUserVerifiedPassword->setText( config->userPasswordSecondary() );
    connect( ui->textBoxUserVerifiedPassword, &QLineEdit::textChanged, config, &Config::setUserPasswordSecondary );
    connect( config, &Config::userPasswordSecondaryChanged, ui->textBoxUserVerifiedPassword, &QLineEdit::setText );
    connect( config, &Config::userPasswordStatusChanged, this, &UsersPage::reportUserPasswordStatus );

    ui->textBoxRootPassword->setText( config->rootPassword() );
    connect( ui->textBoxRootPassword, &QLineEdit::textChanged, config, &Config::setRootPassword );
    connect( config, &Config::rootPasswordChanged, ui->textBoxRootPassword, &QLineEdit::setText );
    ui->textBoxVerifiedRootPassword->setText( config->rootPasswordSecondary() );
    connect( ui->textBoxVerifiedRootPassword, &QLineEdit::textChanged, config, &Config::setRootPasswordSecondary );
    connect( config, &Config::rootPasswordSecondaryChanged, ui->textBoxVerifiedRootPassword, &QLineEdit::setText );
    connect( config, &Config::rootPasswordStatusChanged, this, &UsersPage::reportRootPasswordStatus );

    // deepin-installer merges the full name and the login name: the login
    // name doubles as the full name
    connect( ui->textBoxLoginName, &QLineEdit::textEdited, m_config, &Config::setFullName );

    // If the hostname is going to be written out, then show the field
    if ( ( m_config->hostnameAction() == HostNameAction::EtcHostname )
         || ( m_config->hostnameAction() == HostNameAction::SystemdHostname ) )
    {
        ui->textBoxHostname->setText( config->hostname() );
        connect( ui->textBoxHostname, &QLineEdit::textEdited, config, &Config::setHostName );
        connect( config,
                 &Config::hostnameChanged,
                 [ this ]( const QString& name )
                 {
                     if ( !ui->textBoxHostname->hasFocus() )
                     {
                         ui->textBoxHostname->setText( name );
                     }
                 } );
        connect( config, &Config::hostnameStatusChanged, this, &UsersPage::reportHostNameStatus );
    }
    else
    {
        // Need to hide the hostname parts individually because there's no widget-group
        ui->hostnameLabel->hide();
        ui->labelHostname->hide();
        ui->textBoxHostname->hide();
        ui->labelHostnameError->hide();
    }

    ui->textBoxLoginName->setText( config->loginName() );
    connect( ui->textBoxLoginName, &QLineEdit::textEdited, config, &Config::setLoginName );
    connect( config, &Config::loginNameChanged, ui->textBoxLoginName, &QLineEdit::setText );
    connect( config, &Config::loginNameStatusChanged, this, &UsersPage::reportLoginNameStatus );

    ui->checkBoxDoAutoLogin->setVisible( m_config->displayAutoLogin() );
    ui->checkBoxDoAutoLogin->setChecked( m_config->doAutoLogin() );
    connect( ui->checkBoxDoAutoLogin,
             Calamares::checkBoxStateChangedSignal,
             this,
             [ this ]( Calamares::checkBoxStateType checked )
             { m_config->setAutoLogin( checked != Calamares::checkBoxUncheckedValue ); } );
    connect( config, &Config::autoLoginChanged, ui->checkBoxDoAutoLogin, &QCheckBox::setChecked );

    ui->checkBoxReusePassword->setVisible( m_config->writeRootPassword() );
    ui->checkBoxReusePassword->setChecked( m_config->reuseUserPasswordForRoot() );
    if ( m_config->writeRootPassword() )
    {
        connect( config, &Config::reuseUserPasswordForRootChanged, ui->checkBoxReusePassword, &QCheckBox::setChecked );
        connect( ui->checkBoxReusePassword, Calamares::checkBoxStateChangedSignal, this, &UsersPage::onReuseUserPasswordChanged );
    }

    ui->checkBoxRequireStrongPassword->setVisible( m_config->permitWeakPasswords() );
    ui->checkBoxRequireStrongPassword->setChecked( m_config->requireStrongPasswords() );
    if ( m_config->permitWeakPasswords() )
    {
        connect( ui->checkBoxRequireStrongPassword,
                 Calamares::checkBoxStateChangedSignal,
                 this,
                 [ this ]( int checked ) { m_config->setRequireStrongPasswords( checked != Qt::Unchecked ); } );
        connect(
            config, &Config::requireStrongPasswordsChanged, ui->checkBoxRequireStrongPassword, &QCheckBox::setChecked );
    }

    // Active Directory is not checked or enabled by default
    ui->useADCheckbox->setVisible( m_config->getActiveDirectoryEnabled() );
    onActiveDirectoryToggled( false );

    connect( ui->useADCheckbox, &QCheckBox::toggled, this, &UsersPage::onActiveDirectoryToggled );
    connect( ui->domainField, &QLineEdit::textChanged, config, &Config::setActiveDirectoryDomain );
    connect( ui->domainAdminField, &QLineEdit::textChanged, config, &Config::setActiveDirectoryAdminUsername );
    connect( ui->domainPasswordField, &QLineEdit::textChanged, config, &Config::setActiveDirectoryAdminPassword );
    connect( ui->ipAddressField, &QLineEdit::textChanged, config, &Config::setActiveDirectoryIP );

    CALAMARES_RETRANSLATE_SLOT( &UsersPage::retranslate );

    onReuseUserPasswordChanged( m_config->reuseUserPasswordForRoot() );
    onFullNameTextEdited( m_config->fullName() );
    reportLoginNameStatus( m_config->loginNameStatus() );
    reportHostNameStatus( m_config->hostnameStatus() );

    ui->textBoxLoginName->setEnabled( m_config->isEditable( "loginName" ) );
    ui->textBoxFullName->setEnabled( m_config->isEditable( "fullName" ) );

    retranslate();
}

UsersPage::~UsersPage()
{
    delete ui;
}

void
UsersPage::retranslate()
{
    ui->retranslateUi( this );
    m_avatarButton->setToolTip( tr( "Select an avatar for your account", "@tooltip" ) );
    if ( Calamares::Settings::instance()->isSetupMode() )
    {
        ui->textBoxLoginName->setToolTip( tr( "<small>If more than one person will "
                                              "use this computer, you can create multiple "
                                              "accounts after setup.</small>" ) );
    }
    else
    {
        ui->textBoxLoginName->setToolTip( tr( "<small>If more than one person will "
                                              "use this computer, you can create multiple "
                                              "accounts after installation.</small>" ) );
    }

    const auto up = m_config->userPasswordStatus();
    reportUserPasswordStatus( up.first, up.second );
    const auto rp = m_config->rootPasswordStatus();
    reportRootPasswordStatus( rp.first, rp.second );

    // Keep the detached labels hidden (deepin-installer merged form)
    ui->labelWhatIsYourName->hide();
    ui->username_label_2->hide();
    ui->hostnameLabel->hide();
    ui->password_label_2->hide();
    ui->labelChooseRootPassword->hide();
    ui->textBoxFullName->hide();
    ui->labelFullName->hide();
    ui->labelFullNameError->hide();
}

void
UsersPage::onActivate()
{
    ui->textBoxFullName->setFocus();
    const auto up = m_config->userPasswordStatus();
    reportUserPasswordStatus( up.first, up.second );
    const auto rp = m_config->rootPasswordStatus();
    reportRootPasswordStatus( rp.first, rp.second );

    // Keep the detached labels hidden (deepin-installer merged form)
    ui->labelWhatIsYourName->hide();
    ui->username_label_2->hide();
    ui->hostnameLabel->hide();
    ui->password_label_2->hide();
    ui->labelChooseRootPassword->hide();
    ui->textBoxFullName->hide();
    ui->labelFullName->hide();
    ui->labelFullNameError->hide();
}

void
UsersPage::onFullNameTextEdited( const QString& fullName )
{
    Q_UNUSED( fullName )
}

void
UsersPage::reportLoginNameStatus( const QString& status )
{
    // An empty field shows no decoration at all
    const FieldState state = ui->textBoxLoginName->text().isEmpty()
        ? FieldState::None
        : ( status.isEmpty() ? FieldState::Ok : FieldState::Error );
    setFieldStatus( ui->textBoxLoginName, state, status );
}

void
UsersPage::reportHostNameStatus( const QString& status )
{
    const FieldState state = ui->textBoxHostname->text().isEmpty()
        ? FieldState::None
        : ( status.isEmpty() ? FieldState::Ok : FieldState::Error );
    setFieldStatus( ui->textBoxHostname, state, status );
}

void
UsersPage::reportRootPasswordStatus( int validity, const QString& message )
{
    // When the root password fields are hidden (setRootPassword: false,
    // or "reuse" is checked), do not show any status for them either.
    if ( !ui->textBoxRootPassword->isVisible() )
    {
        setFieldStatus( ui->textBoxRootPassword, FieldState::None, QString() );
        setFieldStatus( ui->textBoxVerifiedRootPassword, FieldState::None, QString() );
        return;
    }
    const bool empty = ui->textBoxRootPassword->text().isEmpty();
    const FieldState state = empty
        ? FieldState::None
        : ( validity == Config::PasswordValidity::Valid ? FieldState::Ok : FieldState::Error );
    setFieldStatus( ui->textBoxRootPassword, state, message );
    setFieldStatus( ui->textBoxVerifiedRootPassword, state, message );
}

void
UsersPage::reportUserPasswordStatus( int validity, const QString& message )
{
    const bool empty = ui->textBoxUserPassword->text().isEmpty();
    const FieldState state = empty
        ? FieldState::None
        : ( validity == Config::PasswordValidity::Valid ? FieldState::Ok : FieldState::Error );
    setFieldStatus( ui->textBoxUserPassword, state, message );
    setFieldStatus( ui->textBoxUserVerifiedPassword, state, message );
}

QIcon
UsersPage::statusIcon( bool ok ) const
{
    const int size = 24;
    QPixmap pm( size, size );
    pm.fill( Qt::transparent );
    QPainter p( &pm );
    p.setRenderHint( QPainter::Antialiasing );
    // translucent white disc, deepin-installer style
    p.setPen( Qt::NoPen );
    p.setBrush( QColor( 255, 255, 255, 70 ) );
    p.drawEllipse( 0, 0, size, size );
    // white tick or cross
    QPen pen( Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin );
    p.setPen( pen );
    if ( ok )
    {
        QPainterPath path;
        path.moveTo( 7, 12.5 );
        path.lineTo( 10.5, 16 );
        path.lineTo( 17, 8.5 );
        p.drawPath( path );
    }
    else
    {
        p.drawLine( 8, 8, 16, 16 );
        p.drawLine( 16, 8, 8, 16 );
    }
    return QIcon( pm );
}

void
UsersPage::setFieldStatus( QLineEdit* edit, FieldState state, const QString& message )
{
    const FieldUi field = m_fields.value( edit );
    if ( !field.glow || !field.statusAction )
    {
        return;
    }
    switch ( state )
    {
    case FieldState::Ok:
        field.glow->setColor( QColor( 76, 175, 80, 170 ) );  // green glow
        field.statusAction->setIcon( statusIcon( true ) );
        field.statusAction->setVisible( true );
        edit->setProperty( "state", QStringLiteral( "ok" ) );
        break;
    case FieldState::Error:
        field.glow->setColor( QColor( 230, 67, 64, 190 ) );  // red glow
        field.statusAction->setIcon( statusIcon( false ) );
        field.statusAction->setVisible( true );
        edit->setProperty( "state", QStringLiteral( "error" ) );
        break;
    case FieldState::None:
    default:
        field.glow->setColor( QColor( 0, 0, 0, 0 ) );
        field.statusAction->setVisible( false );
        edit->setProperty( "state", QString() );
        break;
    }
    // Translucent error bubble below the field
    if ( field.tipLabel )
    {
        field.tipLabel->setText( message );
        field.tipLabel->setVisible( state == FieldState::Error && !message.isEmpty() );
    }
    // Re-evaluate the property-dependent style sheet rules
    edit->style()->unpolish( edit );
    edit->style()->polish( edit );
}

void
UsersPage::onReuseUserPasswordChanged( const int checked )
{
    // Pass the change on to config
    m_config->setReuseUserPasswordForRoot( checked != Qt::Unchecked );
    /* When "reuse" is checked, hide the fields for explicitly
     * entering the root password. However, if we're going to
     * disable the root password anyway, hide them all regardless of
     * the checkbox -- so when writeRoot is false, visible needs
     * to be false, to hide them all.
     *
     * In principle this is only connected when writeRootPassword is @c true,
     * but it is **always** called at least once in the constructor
     * to set up initial visibility.
     */
    const bool visible = m_config->writeRootPassword() ? !checked : false;
    // labelChooseRootPassword stays hidden: the line-edit placeholders
    // carry the meaning (deepin-installer style, merged fields)
    ui->labelRootPassword->setVisible( visible );
    ui->labelRootPasswordError->setVisible( visible );
    ui->textBoxRootPassword->setVisible( visible );
    ui->textBoxVerifiedRootPassword->setVisible( visible );
}

void
UsersPage::onActiveDirectoryToggled( bool checked )
{
    ui->domainLabel->setVisible( checked );
    ui->domainField->setVisible( checked );
    ui->domainAdminLabel->setVisible( checked );
    ui->domainAdminField->setVisible( checked );
    ui->domainPasswordField->setVisible( checked );
    ui->domainPasswordLabel->setVisible( checked );
    ui->ipAddressField->setVisible( checked );
    ui->ipAddressLabel->setVisible( checked );

    m_config->setActiveDirectoryUsed( checked );
}

QPixmap
UsersPage::roundedAvatar( const QPixmap& source, int size ) const
{
    QPixmap scaled = source.scaled( size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
    const QRect crop( ( scaled.width() - size ) / 2, ( scaled.height() - size ) / 2, size, size );

    QPixmap result( size, size );
    result.fill( Qt::transparent );
    QPainter p( &result );
    p.setRenderHint( QPainter::Antialiasing );
    QPainterPath path;
    path.addEllipse( 0, 0, size, size );
    p.setClipPath( path );
    p.drawPixmap( 0, 0, scaled.copy( crop ) );
    return result;
}

namespace
{
/** @brief Full-screen overlay that re-paints the branding wallpaper,
 *
 * so picking an avatar looks like a dedicated page (deepin-installer
 * style) instead of a translucent layer on top of the form.
 */
class AvatarPickerOverlay : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent( QPaintEvent* ) override
    {
        QPainter p( this );
        QString path = Calamares::Branding::instance()->imagePath( Calamares::Branding::ImageEntry::ProductWallpaper );
        if ( !path.isEmpty() )
        {
            path = QDir( Calamares::Branding::instance()->componentDirectory() ).absoluteFilePath( path );
        }
        QPixmap pixmap;
        if ( !path.isEmpty() && QFile::exists( path ) )
        {
            pixmap = QPixmap( path ).scaled( size(), Qt::KeepAspectRatioByExpanding );
        }
        if ( !pixmap.isNull() )
        {
            p.drawPixmap( ( width() - pixmap.width() ) / 2, ( height() - pixmap.height() ) / 2, pixmap );
        }
        else
        {
            p.fillRect( rect(), QColor( 40, 40, 40 ) );
        }
    }
};

///@brief The embedded default avatar (neutral user silhouette)
QPixmap
defaultAvatarPixmap( int size )
{
    return QPixmap( QStringLiteral( ":/images/default-avatar.svg" ) ).scaled( size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
}
}  // namespace

void
UsersPage::updateAvatarIcon()
{
    QPixmap pm;
    const QString path = m_config->userAvatar();
    if ( !path.isEmpty() && QFile::exists( path ) )
    {
        pm = QPixmap( path );
    }
    if ( pm.isNull() )
    {
        // The package's default.png (falls back to the embedded silhouette)
        pm = QPixmap( Config::userAvatarDefault() );
    }
    if ( pm.isNull() )
    {
        pm = defaultAvatarPixmap( 96 );
    }
    if ( !pm.isNull() )
    {
        m_avatarButton->setIcon( roundedAvatar( pm, m_avatarButton->iconSize().width() ) );
    }
}

void
UsersPage::pickAvatar()
{
    // deepin-installer style: the avatar picker is a full-size overlay
    // inside the installer page, not a small popup window
    if ( !m_avatarOverlay )
    {
        m_avatarOverlay = new AvatarPickerOverlay( this->window() );
        m_avatarOverlay->setObjectName( "avatarOverlay" );

        auto* overlayLayout = new QVBoxLayout( m_avatarOverlay );
        overlayLayout->setContentsMargins( 0, 0, 0, 0 );
        overlayLayout->setSpacing( 20 );

        auto* titleLabel = new QLabel( tr( "Select an avatar", "@title" ), m_avatarOverlay );
        titleLabel->setObjectName( "deepinTitle" );
        titleLabel->setAlignment( Qt::AlignHCenter );
        overlayLayout->addWidget( titleLabel );

        auto* list = new QListWidget( m_avatarOverlay );
        list->setObjectName( "avatarList" );
        list->setIconSize( QSize( 96, 96 ) );
        list->setViewMode( QListView::IconMode );
        list->setResizeMode( QListView::Adjust );
        list->setMovement( QListView::Static );
        list->setGridSize( QSize( 130, 140 ) );
        list->setUniformItemSizes( true );
        list->setWordWrap( false );
        list->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
        list->setMinimumWidth( 820 );
        list->setMaximumWidth( 980 );
        overlayLayout->addWidget( list, 1, Qt::AlignHCenter );

        auto* backButton = new QPushButton( tr( "Back", "@button" ), m_avatarOverlay );
        backButton->setObjectName( "deepinNavButton" );
        backButton->setFixedSize( 200, 36 );
        overlayLayout->addWidget( backButton, 0, Qt::AlignHCenter );

        connect( backButton, &QPushButton::clicked, m_avatarOverlay, &QWidget::hide );
        // A single click picks the avatar and returns
        connect( list,
                 &QListWidget::itemClicked,
                 this,
                 [ this ]( QListWidgetItem* item )
                 {
                     m_config->setUserAvatar( item->data( Qt::UserRole ).toString() );
                     m_avatarOverlay->hide();
                 } );
    }

    auto* list = m_avatarOverlay->findChild< QListWidget* >();
    list->clear();
    // The picker only offers the numbered faces; default.png is not
    // listed (it duplicates one of them).

    for ( const QString& path : Config::userAvatarCandidates() )
    {
        auto* item = new QListWidgetItem( QIcon( path ), QString(), list );
        item->setData( Qt::UserRole, path );
        item->setToolTip( path );
        if ( path == m_config->userAvatar() )
        {
            list->setCurrentItem( item );
        }
    }

    QWidget* mainWindow = this->window();
    m_avatarOverlay->setGeometry( mainWindow ? mainWindow->rect() : this->rect() );
    m_avatarOverlay->show();
    m_avatarOverlay->raise();
}
