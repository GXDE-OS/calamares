/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITIONEDITORPAGE_H
#define PARTITIONEDITORPAGE_H

#include <QWidget>

/** @brief Base class for the partition editor pages.
 *
 * deepin-installer style: the partition editors are dedicated pages
 * inside the partition module (not dialogs). A page emits finished()
 * when the user confirms or cancels, and the partition module then
 * returns to the main partition page.
 */
class PartitionEditorPage : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

signals:
    ///@brief Emitted when the page is done; @p accepted for OK
    void finished( bool accepted );
};

#endif  // PARTITIONEDITORPAGE_H
