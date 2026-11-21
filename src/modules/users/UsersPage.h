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

#include <QWidget>

class Config;

class QLabel;
class QPixmap;
class QToolButton;

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
    void retranslate();
    ///@brief Draw @p source as a round avatar of the given size
    QPixmap roundedAvatar( const QPixmap& source, int size ) const;

    Ui::Page_UserSetup* ui;
    Config* m_config;

    QToolButton* m_avatarButton = nullptr;
};

#endif  // USERSPAGE_H
