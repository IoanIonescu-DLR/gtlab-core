/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2015 by DLR
 *
 *  Created on: 15.10.2015
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#ifndef GTPACKAGEUI_H
#define GTPACKAGEUI_H

#include "gt_gui_exports.h"

#include "gt_objectui.h"

/**
 * @brief The GtPackageUI class
 */
class GT_GUI_EXPORT GtPackageUI : public GtObjectUI
{
    Q_OBJECT

public:
    /**
     * @brief GtPackageUI
     */
    Q_INVOKABLE GtPackageUI();

    /**
     * Virtual function to specify object specific icon.
     * @return Object specific icon
     */
    virtual QIcon icon(GtObject* obj) const override;

};

#endif // GTPACKAGEUI_H
