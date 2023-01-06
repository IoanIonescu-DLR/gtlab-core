#include <QVBoxLayout>
#include <QPushButton>
#include <QMenu>

#include "gt_object.h"
#include "gt_propertytreeview.h"
#include "gt_application.h"
#include "gt_session.h"
#include "gt_project.h"
#include "gt_propertystructcontainer.h"
#include "gt_icons.h"
#include "gt_stylesheets.h"
#include "gt_propertymodel.h"

#include "gt_propertycontainerwidget.h"

GtPropertyContainerWidget::GtPropertyContainerWidget(
        GtObject* obj, GtPropertyStructContainer& container, QWidget* parent) :
    QWidget(parent)
{
    QVBoxLayout* lay = new QVBoxLayout;

    lay->setContentsMargins(0, 0, 0, 0);

    m_containerTree = new GtPropertyTreeView(gtApp->session());
    m_containerTree->setColumnWidth(0, 100);
    m_containerTree->setColumnWidth(1, 70);
    m_containerTree->setColumnWidth(2, 50);
    m_containerTree->setFrameStyle(QTreeView::NoFrame);

    m_containerTree->setObject(obj, container);
    m_containerTree->setScope(obj->findParent<GtProject*>());

    lay->addWidget(m_containerTree);

    QPushButton* addBtn = new QPushButton(tr("Add new ") +
                                          container.entryPrefix());
    addBtn->setIcon(gt::gui::icon::add16());
    addBtn->setToolTip(tr("Add New Entry"));
    addBtn->setStyleSheet(gt::gui::stylesheet::buttonStyleSheet());

    lay->addWidget(addBtn);

    setLayout(lay);

    m_allowedTypes = container.allowedTypes();
    m_obj = obj;
    m_containerId = container.ident();

    connect(addBtn, SIGNAL(clicked(bool)), SLOT(addNewEntry()));
}

void
GtPropertyContainerWidget::addNewEntry()
{
    if (m_allowedTypes.isEmpty())
    {
        gtWarning() << tr("no types registered!");
        return;
    }

    if (!m_obj)
    {
        gtWarning() << tr("invalid object!");
        return;
    }

    auto* container = m_obj->findPropertyContainer(m_containerId);

    if (!container)
    {
        gtWarning() << tr("container not found!");
        return;
    }

    // only single type allows. add default entry
    if (m_allowedTypes.size() == 1)
    {
        addNewEntry(*container, m_allowedTypes.first());
        return;
    }

    QMenu menu(this);

    QList<QAction*> actions;

    foreach (const QString& allowedType, m_allowedTypes)
    {
        actions << menu.addAction(allowedType);
    }

    QAction* a = menu.exec(QCursor::pos());

    const int a_idx = actions.indexOf(a);

    if (a_idx >= 0)
    {
        addNewEntry(*container, m_allowedTypes.at(a_idx));
    }
}

void
GtPropertyContainerWidget::addNewEntry(GtPropertyStructContainer& container,
                                       const QString& entryType)
{
    GtPropertyModel* model = m_containerTree->propertyModel();

    if (model)
    {
        QModelIndex idx = model->addNewStructContainerEntry(
                    container, entryType);

        if (idx.isValid())
        {
            QModelIndex fidx = m_containerTree->mapFromSource(idx);
            m_containerTree->expandRecursively(fidx);
        }
    }
}
