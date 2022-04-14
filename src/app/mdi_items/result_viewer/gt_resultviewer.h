/* GTlab - Gas Turbine laboratory
 * Source File: gt_resultviewer.h
 * copyright 2009-2016 by DLR
 *
 *  Created on: 04.08.2016
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#ifndef GTRESULTVIEWER_H
#define GTRESULTVIEWER_H

#include "gt_mdiitem.h"

class QLabel;
class GtDataZoneModel;
class QComboBox;

/**
 * @brief The GtResultViewer class
 */
class GtResultViewer : public GtMdiItem
{
    Q_OBJECT

public:
    /**
     * @brief GtResultViewer
     */
    Q_INVOKABLE GtResultViewer();

    ~GtResultViewer() override;

    /**
     * @brief Virtual function to specify item specific icon.
     * @return Object specific icon.
     */
    QIcon icon() const override;

    /**
     * @brief setData
     * @param obj
     */
    void setData(GtObject* obj) override;

public slots:

    void axSelectorChanged(int index);

    void tickSelectorChanged(int index);

private:
    /// Label
    QLabel* m_label;

    /// Model
    GtDataZoneModel* m_model;

    QComboBox* m_axSelection;

    QComboBox* m_tickSelection;

    QLabel* m_fixedValueLabel;
};

#endif // GTRESULTVIEWER_H
