/* GTlab - Gas Turbine laboratory
 * Source File: gt_objectfiltermodel.h
 * copyright 2009-2016 by DLR
 *
 *  Created on: 19.05.2016
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#ifndef GTOBJECTFILTERMODEL_H
#define GTOBJECTFILTERMODEL_H

#include "gt_core_exports.h"

#include "gt_treefiltermodel.h"

/**
 * @brief The GtObjectFilterModel class
 */
class GT_CORE_EXPORT GtObjectFilterModel : public GtTreeFilterModel
{
    Q_OBJECT

public:
    /**
     * @brief GtObjectFilterModel
     * @param parent
     */
    explicit GtObjectFilterModel(QObject* parent = nullptr);

    /**
     * @brief setFilterData
     * @param val
     */
    void setFilterData(const QStringList& val);

protected:
    /**
     * @brief filterAcceptsRow
     * @param source_row
     * @param source_parent
     * @return
     */
    virtual bool filterAcceptsRow(int source_row,
                                  const QModelIndex& source_parent) const override;

    /**
     * @brief flags
     * @param index
     * @return
     */
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    /// Filter data.
    QStringList m_filterData;

    /**
     * @brief acceptRow
     * @param str
     * @return
     */
    bool acceptsRow(const char* ident) const;

};

#endif // GTOBJECTFILTERMODEL_H
