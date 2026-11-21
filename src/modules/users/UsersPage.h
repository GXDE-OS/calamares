/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2007 Free Software Foundation, Inc.
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Portions from the Manjaro Installation Framework
 *   by Roland Singer <roland@manjaro.org>
 *   Copyright (C) 2007 Free Software Foundation, Inc.
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef USERSPAGE_H
#define USERSPAGE_H

#include <QHash>
#include <QWidget>

class QAction;
class Config;
class QGraphicsDropShadowEffect;
class QLabel;
class QResizeEvent;
class QLineEdit;
class QPixmap;
class QToolButton;
class QWidget;

namespace Ui
{
class Page_UserSetup;
}  // namespace Ui

class UsersPage : public QWidget
{
    Q_OBJECT
public:
    explicit UsersPage( Config* config, QWidget* parent = nullptr );
    ~UsersPage() override;

    void onActivate();

protected:
    void resizeEvent( QResizeEvent* event ) override;

    protected slots:
    void onFullNameTextEdited( const QString& );
    void reportLoginNameStatus( const QString& );
    void reportHostNameStatus( const QString& );
    void onReuseUserPasswordChanged( const int );
    void reportUserPasswordStatus( int, const QString& );
    void reportRootPasswordStatus( int, const QString& );

    void onActiveDirectoryToggled( bool checked );

    /** @brief Show the avatar picker and store the chosen avatar */
    void pickAvatar();
    ///@brief Refresh the avatar button's icon from the config
    void updateAvatarIcon();

private:
    ///@brief Visual state of one line edit (deepin-installer style)
    enum class FieldState
    {
        None,  ///< no input yet, no decoration
        Ok,  ///< green glow + check icon
        Error  ///< red glow + cross icon, message as tooltip
    };

    struct FieldUi
    {
        QGraphicsDropShadowEffect* glow = nullptr;
        QAction* statusAction = nullptr;
        QLabel* tipLabel = nullptr;
    };

    void retranslate();
    ///@brief Draw @p source as a round avatar of the given size
    QPixmap roundedAvatar( const QPixmap& source, int size ) const;
    ///@brief White-on-translucent tick / cross icon, deepin style
    QIcon statusIcon( bool ok ) const;
    ///@brief Apply glow, border state and status icon to a line edit
    void setFieldStatus( QLineEdit* edit, FieldState state, const QString& message );

    Ui::Page_UserSetup* ui;
    Config* m_config;

    QToolButton* m_avatarButton = nullptr;
    QWidget* m_avatarOverlay = nullptr;

    QHash< QLineEdit*, FieldUi > m_fields;
};

#endif  // USERSPAGE_H
