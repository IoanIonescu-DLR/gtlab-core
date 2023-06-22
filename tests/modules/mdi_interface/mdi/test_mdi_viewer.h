/* GTlab - Gas Turbine laboratory
 *
 * SPDX-License-Identifier: MPL-2.0+
 * SPDX-FileCopyrightText: 2023 German Aerospace Center (DLR)
 *
 * Created on: 12.08.2020
 * Author: M. Bröcker
 */

#ifndef TESTMDIVIEWER_H
#define TESTMDIVIEWER_H

#include "gt_mdiitem.h"


class GtGraphicsView;
/**
 * @brief The TestMdiViewer class
 */
class TestMdiViewer : public GtMdiItem
{
    Q_OBJECT

public:

    /**
     * @brief Constructor.
     */
    Q_INVOKABLE TestMdiViewer();
    ~TestMdiViewer() override;

    /**
     * @brief Test that only one instance of this item can be created
     * @return false
     */
    bool allowsMultipleInstances() const override;

private:

    /// Graphics view to test basic functionality
    GtGraphicsView* m_view;
};

#endif // TESTMDIVIEWER_H
