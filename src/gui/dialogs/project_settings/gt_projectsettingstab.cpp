/* GTlab - Gas Turbine laboratory
 * Source File: gt_projectsettingstab.h
 * copyright 2009-2021 by DLR
 *
 *  Created on: 31.03.2022
 *  Author: Marius Bröcker (AT-TWK)
 *  Tel.: +49 2203 601 5199
 */

#include "gt_projectsettingstab.h"

#include <QVBoxLayout>

#include "gt_project.h"

GtProjectSettingsTab::GtProjectSettingsTab(GtProject* project,
                                           QWidget* parent) :
    QWidget{parent},
    m_project{project},
    m_layout{new QVBoxLayout{this}}
{

}

QVBoxLayout*
GtProjectSettingsTab::layout() const
{
    return m_layout;
}

GtProject*
GtProjectSettingsTab::project() const
{
    return m_project.data();
}
