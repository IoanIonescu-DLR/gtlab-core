/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2022 by DLR
 *
 *  Created on: 08.06.2022
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include <QVBoxLayout>
#include <QRadioButton>
#include <QLineEdit>

#include "gt_project.h"
#include "gt_projectspecwidget.h"
#include "gt_projectupgradedialog.h"
#include "gt_datamodel.h"
#include "gt_application.h"

#include "gt_projectupgradesettingspage.h"

GtProjectUpgradeSettingsPage::GtProjectUpgradeSettingsPage(GtProject* project,
                                             GtProjectUpgradeDialog* parent) :
    QWizardPage(parent),
    m_specWid(new GtProjectSpecWidget),
    m_wiz(parent)
{
    setTitle(tr("Upgrade Settings"));

    QVBoxLayout* layout = new QVBoxLayout;

    m_overwrite =
            new QRadioButton(tr("Overwrite the existing project data.\n"
                                "A backup of all relevant files is created in "
                                "the project folder (/backup)."));
    m_overwrite->setChecked(true);

    QRadioButton* saveNew =
            new QRadioButton(tr("Save as a new project."));

    layout->addWidget(m_overwrite);
    layout->addWidget(saveNew);
    layout->addWidget(m_specWid);

    m_specWid->setVisible(false);

    QString id = gtDataModel->uniqueObjectName(
                project->objectName() + "_upgraded",
                gtApp->session());

    m_specWid->setProjectInformation(id, m_specWid->pathLine()->text());

    QObject::connect(saveNew, SIGNAL(toggled(bool)), m_specWid,
                     SLOT(setVisible(bool)));

    layout->addStretch(1);

    setLayout(layout);
}

bool
GtProjectUpgradeSettingsPage::validatePage()
{
    m_wiz->updateSettings(m_overwrite->isChecked(),
                          m_specWid->nameLine()->text(),
                          m_specWid->pathLine()->text());

    return true;
}
