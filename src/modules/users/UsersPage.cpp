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

#include <QBoxLayout>
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

/** @brief Add an error message and pixmap to a label. */
static inline void
labelError( QLabel* pix, QLabel* label, Calamares::ImageType icon, const QString& message )
{
    label->setText( message );
    pix->setPixmap( Calamares::defaultPixmap( icon, Calamares::Original, pix->size() ) );
    // Hide when there is nothing to say, to keep the form compact
    const bool show = !message.isEmpty();
    pix->setVisible( show );
    label->setVisible( show );
}

/** @brief Clear error, set happy pixmap on a label to indicate "ok". */
static inline void
labelOk( QLabel* pix, QLabel* label )
{
    label->clear();
    pix->setPixmap( Calamares::defaultPixmap( Calamares::StatusOk, Calamares::Original, pix->size() ) );
    pix->setVisible( true );
    label->setVisible( false );
}

/** @brief Sets error or ok on a label depending on @p status and @p value
 *
 * - An **empty** @p value gets no message and no icon.
 * - A non-empty @p value, with an **empty** @p status gets an "ok".
 * - A non-empty @p value with a non-empty @p status gets an error indicator.
 */
static inline void
labelStatus( QLabel* pix, QLabel* label, const QString& value, const QString& status )
{
    if ( status.isEmpty() )
    {
        if ( value.isEmpty() )
        {
            // This is different from labelOK() because no checkmark is shown
            label->clear();
            pix->clear();
            pix->setVisible( false );
            label->setVisible( false );
        }
        else
        {
            labelOk( pix, label );
        }
    }
    else
    {
        labelError( pix, label, Calamares::ImageType::StatusError, status );
    }
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
}  // namespace

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

    // A field = the edit centered (labels are hidden, placeholders carry
    // the meaning, deepin-installer style) and a compact status row
    // (icon + error) that hides itself when empty.
    auto addField = [ this, fieldWidth ]( QLabel* label, QLineEdit* edit, QLabel* iconLabel, QLabel* errorLabel )
    {
        Q_UNUSED( label )
        edit->setMinimumWidth( fieldWidth );
        edit->setMaximumWidth( fieldWidth );
        auto* editRow = new QHBoxLayout;
        editRow->addStretch();
        editRow->addWidget( edit );
        editRow->addStretch();
        ui->verticalLayout_12->addLayout( editRow );

        iconLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        errorLabel->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
        errorLabel->setMaximumWidth( 480 );
        auto* statusRow = new QHBoxLayout;
        statusRow->setSpacing( 6 );
        statusRow->addStretch();
        statusRow->addWidget( iconLabel );
        statusRow->addWidget( errorLabel );
        statusRow->addStretch();
        ui->verticalLayout_12->addLayout( statusRow );
    };

    // Like addField, but with two edits stacked vertically (password and
    // its confirmation) sharing the same column.
    auto addStackedField = [ this, fieldWidth ]( QLabel* label,
                                                 QLineEdit* first,
                                                 QLineEdit* second,
                                                 QLabel* iconLabel,
                                                 QLabel* errorLabel )
    {
        Q_UNUSED( label )
        for ( auto* edit : { first, second } )
        {
            edit->setMinimumWidth( fieldWidth );
            edit->setMaximumWidth( fieldWidth );
            auto* editRow = new QHBoxLayout;
            editRow->addStretch();
            editRow->addWidget( edit );
            editRow->addStretch();
            ui->verticalLayout_12->addLayout( editRow );
        }

        iconLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        errorLabel->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
        errorLabel->setMaximumWidth( 480 );
        auto* statusRow = new QHBoxLayout;
        statusRow->setSpacing( 6 );
        statusRow->addStretch();
        statusRow->addWidget( iconLabel );
        statusRow->addWidget( errorLabel );
        statusRow->addStretch();
        ui->verticalLayout_12->addLayout( statusRow );
    };

    ui->verticalLayout_12->setSpacing( 10 );
    ui->verticalLayout_12->addWidget( m_avatarButton, 0, Qt::AlignHCenter );
    ui->verticalLayout_12->addSpacing( 10 );

    // deepin-installer merges the full name and the login name into a
    // single field: hide the separate full-name field and mirror the
    // login name into the full name. Labels are hidden as well; the
    // line-edit placeholders carry the meaning.
    ui->labelWhatIsYourName->hide();
    ui->textBoxFullName->hide();
    ui->labelFullName->hide();
    ui->labelFullNameError->hide();
    ui->username_label_2->hide();
    ui->hostnameLabel->hide();
    ui->password_label_2->hide();
    ui->labelChooseRootPassword->hide();
    addField( ui->username_label_2, ui->textBoxLoginName, ui->labelUsername, ui->labelUsernameError );
    addField( ui->hostnameLabel, ui->textBoxHostname, ui->labelHostname, ui->labelHostnameError );
    addStackedField( ui->password_label_2,
                     ui->textBoxUserPassword,
                     ui->textBoxUserVerifiedPassword,
                     ui->labelUserPassword,
                     ui->labelUserPasswordError );
    addStackedField( ui->labelChooseRootPassword,
                     ui->textBoxRootPassword,
                     ui->textBoxVerifiedRootPassword,
                     ui->labelRootPassword,
                     ui->labelRootPasswordError );

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
    labelStatus( ui->labelFullName, ui->labelFullNameError, fullName, QString() );
}

void
UsersPage::reportLoginNameStatus( const QString& status )
{
    labelStatus( ui->labelUsername, ui->labelUsernameError, m_config->loginName(), status );
}

void
UsersPage::reportHostNameStatus( const QString& status )
{
    labelStatus( ui->labelHostname, ui->labelHostnameError, m_config->hostname(), status );
}

static inline void
passwordStatus( QLabel* iconLabel, QLabel* messageLabel, int validity, const QString& message )
{
    switch ( validity )
    {
    case Config::PasswordValidity::Valid:
        labelOk( iconLabel, messageLabel );
        break;
    case Config::PasswordValidity::Weak:
        labelError( iconLabel, messageLabel, Calamares::StatusWarning, message );
        break;
    case Config::PasswordValidity::Invalid:
    default:
        labelError( iconLabel, messageLabel, Calamares::StatusError, message );
        break;
    }
}

void
UsersPage::reportRootPasswordStatus( int validity, const QString& message )
{
    // When the root password fields are hidden (setRootPassword: false,
    // or "reuse" is checked), do not show any status for them either.
    const bool rootFieldsVisible = ui->textBoxRootPassword->isVisible();
    if ( !rootFieldsVisible )
    {
        ui->labelRootPassword->hide();
        ui->labelRootPasswordError->hide();
        return;
    }
    passwordStatus( ui->labelRootPassword, ui->labelRootPasswordError, validity, message );
}

void
UsersPage::reportUserPasswordStatus( int validity, const QString& message )
{
    passwordStatus( ui->labelUserPassword, ui->labelUserPasswordError, validity, message );
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
    auto* defaultItem = new QListWidgetItem( QIcon( Config::userAvatarDefault() ), QString(), list );
    defaultItem->setData( Qt::UserRole, QString() );
    defaultItem->setToolTip( tr( "Default", "@option" ) );
    if ( m_config->userAvatar().isEmpty() )
    {
        list->setCurrentItem( defaultItem );
    }

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
