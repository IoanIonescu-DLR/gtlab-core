/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2015 by DLR
 *
 *  Created on: 11.10.2015
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include <QKeyEvent>

#include "gt_listview.h"
#include "gt_application.h"

GtListView::GtListView(QWidget* parent) : QListView(parent)
{

}

void
GtListView::keyPressEvent(QKeyEvent* event)
{
    if (gtApp->compareKeyEvent(event, "search"))
    {
        emit searchRequest();
    }
    else if (gtApp->compareKeyEvent(event, "copy"))
    {
        emit copyRequest();
    }
    else if (gtApp->compareKeyEvent(event, "delete"))
    {
        emit deleteRequest();
    }

    return QListView::keyPressEvent(event);
}

