/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2018-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITIONPAGE_H
#define PARTITIONPAGE_H

#include <QMutex>
#include <QScopedPointer>
#include <functional>
#include <QWidget>

class Config;
class QStackedLayout;
class PartitionEditorPage;
class PartitionCoreModule;
class Ui_PartitionPage;

class Device;
class Partition;

/**
 * The user interface for the module.
 *
 * Shows the information exposed by PartitionCoreModule and asks it to schedule
 * jobs according to user actions.
 */
class PartitionPage : public QWidget
{
    Q_OBJECT
public:
    explicit PartitionPage( PartitionCoreModule* core, const Config & config, QWidget* parent = nullptr );
    ~PartitionPage() override;

signals:
    ///@brief The internal editor page opened (true) or closed (false)
    void editorPageActive( bool active );

    void onRevertClicked();

    int selectedDeviceIndex();
    void selectDeviceByIndex( int index );

private Q_SLOTS:
    /// @brief Update everything when the base device changes
    void updateFromCurrentDevice();
    /// @brief Update when the selected device for boot loader changes
    void updateBootLoaderInstallPath();
    /// @brief Explicitly selected boot loader path
    void updateSelectedBootLoaderIndex();
    /// @brief After boot loader model changes, try to preserve previously set value
    void restoreSelectedBootLoader();
    /// @brief Make the selections in each widget match
    void reconcileSelections();

private:
    QScopedPointer< Ui_PartitionPage > m_ui;
    PartitionCoreModule* m_core;

    /** deepin-installer style: the partition editors open as dedicated
     *  pages inside this stack, on top of the main partition page */
    QStackedLayout* m_pageStack = nullptr;
    void presentEditorPage( PartitionEditorPage* page, const std::function< void() >& onAccepted );
    void updateButtons();
    void onNewPartitionTableClicked();
    void onNewVolumeGroupClicked();
    void onResizeVolumeGroupClicked();
    void onDeactivateVolumeGroupClicked();
    void onRemoveVolumeGroupClicked();
    void onCreateClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onPartitionViewActivated();
    void onPartitionModelReset();

    void updatePartitionToCreate( Device*, Partition* );
    void editExistingPartition( Device*, Partition* );
    void updateBootLoaderIndex();

    /**
     * @brief Check if a new partition can be created (as primary) on the device.
     *
     * Returns true if a new partition can be created on the device. Provides
     * a warning popup and returns false if it cannot.
     */
    bool checkCanCreate( Device* );

    QStringList getCurrentUsedMountpoints();

    QMutex m_revertMutex;
    int m_lastSelectedBootLoaderIndex;
    bool m_isEfi;
};

#endif  // PARTITIONPAGE_H
