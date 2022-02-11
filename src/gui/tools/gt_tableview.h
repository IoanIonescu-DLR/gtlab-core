/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2015 by DLR
 *
 *  Created on: 27.10.2015
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#ifndef GTTABLEVIEW_H
#define GTTABLEVIEW_H

#include "gt_gui_exports.h"

#include <QTableView>

/**
 * @brief The GtTableView class
 */
class GT_GUI_EXPORT GtTableView : public QTableView
{
    Q_OBJECT

public:
    explicit GtTableView(QWidget* parent = nullptr);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

signals:
    void searchRequest();

    void copyRequest();

};

#endif // GTTABLEVIEW_H
