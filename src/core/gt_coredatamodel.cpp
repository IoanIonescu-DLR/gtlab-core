/* GTlab - Gas Turbine laboratory
 *
 * SPDX-License-Identifier: MPL-2.0+
 * SPDX-FileCopyrightText: 2023 German Aerospace Center (DLR)
 * Source File: gt_coredatamodel.cpp
 *
 *  Created on: 02.09.2016
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include <QMimeData>
#include <QDir>

#include "gt_session.h"
#include "gt_project.h"
#include "gt_objectfactory.h"
#include "gt_objectmemento.h"
#include "gt_coreapplication.h"
#include "gt_command.h"
#include "gt_logging.h"
#include "gt_state.h"
#include "gt_statehandler.h"
#include "gt_externalizationmanager.h"
#include "gt_projectanalyzer.h"
#include "gt_versionnumber.h"

#include "gt_coredatamodel.h"

GtCoreDatamodel* GtCoreDatamodel::m_self = 0;

GtCoreDatamodel::GtCoreDatamodel(QObject* parent) : QAbstractItemModel(parent),
    m_session(nullptr)
{
    init();
}

Qt::ItemFlags
GtCoreDatamodel::flags(const QModelIndex& index) const
{
    // check index
    if (!index.isValid())
    {
        return {};
    }

    // collect default flags
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    // get object
    GtObject* object = objectFromIndex(index);

    // check object
    if (object)
    {
        // check if object is renamable
        if (object->isRenamable() && index.column() == 0)
        {
            // add editable flag
            defaultFlags = defaultFlags | Qt::ItemIsEditable;
        }
    }

    // return flags
    return defaultFlags;
}

void
GtCoreDatamodel::setSession(GtSession* session)
{
    beginResetModel();
    m_session = session;
    endResetModel();
}

void
GtCoreDatamodel::appendProjectData(GtProject* project,
                                   QList<GtObject*> const& projectData)
{
    if (projectData.isEmpty())
    {
        gtWarning() << tr("could not open project") << QStringLiteral(" - ") <<
                    tr("project data corrupted!");
        return;
    }

    // get model index corresponding to project
    QModelIndex objIndex = indexFromObject(project);

    // prepare model
    beginInsertRows(objIndex, 0, projectData.size() - 1);

    // append data to project
    project->appendChildren(projectData);

    // revert all changes generated by append procedure
    project->acceptChangesRecursively();

    // notify model
    endInsertRows();

    // update current project in application
    gtApp->setCurrentProject(project);
}

QModelIndex
GtCoreDatamodel::rootParent(const QModelIndex& index) const
{
    // check index
    if (!index.isValid())
    {
        return {};
    }

    // check parent index
    if (!index.parent().isValid())
    {
        // if index has no parent return index
        return index;
    }

    // return parent index recursively
    return rootParent(index.parent());
}

GtCoreDatamodel*
GtCoreDatamodel::instance()
{
    return m_self;
}

void
GtCoreDatamodel::init()
{
    m_self = this;
}

void
GtCoreDatamodel::initProjectStates(GtProject* project)
{
    // initialize externalization states
    GtState* enableState = gtStateHandler->initializeState(project,
                                    QStringLiteral("ExternalizationSettings"),
                                    QStringLiteral("Enable Externalization"),
                                    project->objectPath(),
                                    false, project);

    // initialize last task group state
    gtStateHandler->initializeState(project,
                                    QStringLiteral("Project Settings"),
                                    QStringLiteral("Last Task Group"),
                                    project->objectPath() + ";lastTaskGroup",
                                    QString(), project);

    // set init values
    gtExternalizationManager->enableExternalization(enableState->getValue());

    // update values if states change
    connect(enableState, SIGNAL(valueChanged(const QVariant&)),
            gtExternalizationManager,
            SLOT(enableExternalization(const QVariant&)));
}

GtSession*
GtCoreDatamodel::session()
{
    return m_session;
}

GtProject*
GtCoreDatamodel::currentProject()
{
    // check session
    if (m_session)
    {
        // return current project from session
        return m_session->currentProject();
    }

    // no session -> no project
    return nullptr;
}

GtProject*
GtCoreDatamodel::findProject(const QString& id)
{
    // check session
    if (m_session)
    {
        // return project corresponding to given identification string
        return m_session->findProject(id);
    }

    // no session -> no project
    return nullptr;
}

QList<GtProject*>
GtCoreDatamodel::projects() const
{
    // check session
    if (m_session)
    {
        // return project list of current session
        return m_session->projects();
    }

    // no session -> no projects
    return QList<GtProject*>();
}

QStringList
GtCoreDatamodel::projectIds() const
{
    // check session
    if (m_session)
    {
        // return project identification string list of current session
        return m_session->projectIds();
    }

    // no session -> no projects
    return QStringList();
}

bool
GtCoreDatamodel::openProject(const QString& id)
{
    return openProject(findProject(id));
}

bool
GtCoreDatamodel::openProject(GtProject* project)
{
    // check project pointer
    if (!project)
    {
        gtFatal() << tr("Null Project!");
        return false;
    }

    gtDebug() << tr("Loading project '%1'...").arg(project->objectName());

    // check if project is already open
    if (project->isOpen())
    {
        return false;
    }

    // check whether a project is already open
    if (gtDataModel->currentProject())
    {
        return false;
    }

    // project ready to be opened. check for module updater
    if (project->upgradesAvailable())
    {
        gtError()
                << tr("Project '%1' requires updates to the data structure.")
                   .arg(project->objectName())
                << tr("Run the project data upgrade command first!");
        return false;
    }

    initProjectStates(project);

    // collect project data
    GtObjectList data = m_session->loadProjectData(project);

    appendProjectData(project, data);

    gtInfo() << project->objectName() << tr("loaded!");

    return true;
}

bool
GtCoreDatamodel::saveProject(const QString& id)
{
    return saveProject(findProject(id));
}

bool
GtCoreDatamodel::saveProject(GtProject* project)
{
    bool retval = false;

    // check pointer
    if (!project)
    {
        return retval;
    }

    // check session
    if (m_session)
    {
        // save project data of given project
        retval = m_session->saveProjectData(project);

        if (retval)
        {
            emit projectSaved(project);
        }
    }

    // no session -> no projects
    return retval;
}

bool
GtCoreDatamodel::closeProject(GtProject* project)
{
    // check project
    if (!project)
    {
        return false;
    }

    // check if project is already closed
    if (!project->isOpen())
    {
        return false;
    }

    // check session
    if (!m_session)
    {
        return false;
    }

    // collect entire project data
    GtObjectList list = project->findDirectChildren<GtObject*>();

    // check project data
    if (list.isEmpty())
    {
        return false;
    }

    // get model index of project
    QModelIndex objIndex = indexFromObject(project);

    // prepare model
    beginRemoveRows(objIndex, 0, list.size() - 1);

    // delete project data
    qDeleteAll(list);

    // accept changes generated by delete procedure
    project->acceptChangesRecursively();

    // notify model
    endRemoveRows();

    // check current project
    if (project == m_session->currentProject())
    {
        // switch to another project
        gtApp->switchCurrentProject();
    }

    // return pure success
    return true;
}

bool
GtCoreDatamodel::deleteProject(GtProject* project)
{
    // check project
    if (!project)
    {
        return false;
    }

    // check session
    if (!m_session)
    {
        return false;
    }

    // search for project within current session
    if (!m_session->findProject(project->objectName()))
    {
        gtWarning() << tr("Cannot delete project!") << QStringLiteral(" ") <<
                    tr("Project is not inside current session.");
        return false;
    }

    // check if project is open
    if (project->isOpen())
    {
        gtWarning() << tr("Cannot delete an open project!");
        return false;
    }

    // get model index of project
    QModelIndex objIndex = indexFromObject(project);

    // check model index
    if (!objIndex.isValid())
    {
        return false;
    }

    // prepare model
    beginRemoveRows(QModelIndex(), objIndex.row(), objIndex.row());

    // delete project from session
    m_session->deleteProject(project);

    // notify model
    endRemoveRows();

    // return nothing as success
    return true;
}

bool GtCoreDatamodel::newProject(GtProject* project)
{
    return newProject(project, true);
}


bool
GtCoreDatamodel::newProject(GtProject* project, bool doOpen)
{
    // check project
    if (!project)
    {
        gtDebug() << "GtDataModel::newProject: " << "project == NULL";
        return false;
    }

    // check session
    if (!m_session)
    {
        gtDebug() << "GtDataModel::newProject: " << "session == NULL";
        return false;
    }

    // search for project within session
    if (m_session->findProject(project->objectName()))
    {
        gtWarning() << tr("project already exists in session!");
        return false;
    }

    // check if project is valid
    if (!project->isValid())
    {
        gtError() << tr("could not add project to session: ")
                  << tr("project not valid!");
        return false;
    }

    // get number of projects within session
    const int row = m_session->projects().size();

    // prepare model
    beginInsertRows(QModelIndex(), row, row);

    // add project to current session
    m_session->addProject(project);

    // notify model
    endInsertRows();

    // open added project
    if (doOpen)
    {
        openProject(project->objectName());
    }

    // return success
    return true;
}

int
GtCoreDatamodel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

int
GtCoreDatamodel::rowCount(const QModelIndex& parent) const
{
    // check column
    if (parent.column() > 0)
    {
        return 0;
    }

    if (!m_session)
    {
        return 0;
    }

    if (!parent.isValid())
    {
        return m_session->projects().size();
    }

    // get parent item
    GtObject* parentItem = objectFromIndex(parent);

    if (!parentItem)
    {
        return 0;
    }

    // return number of child objects
    return parentItem->findDirectChildren<GtObject*>().size();
}

QModelIndex
GtCoreDatamodel::index(int row, int col, const QModelIndex& parent) const
{
    // root
    if (!parent.isValid())
    {
        QList<GtProject*> projects = m_session->projects();

        // check array size
        if (row >= projects.size())
        {
            return {};
        }

        // get child object corresponding to row number
        GtObject* childItem = projects[row];

        // check object
        if (!childItem)
        {
            return {};
        }

        // create index
        return createIndex(row, col, childItem);
    }

    // non root
    GtObject* parentItem = objectFromIndex(parent);

    // check parent item
    if (!parentItem)
    {
        return {};
    }

    QList<GtObject*> childItems =
            parentItem->findDirectChildren<GtObject*>();

    // check array size
    if (row >= childItems.size())
    {
        return {};
    }

    // get child object corresponding to row number
    GtObject* childItem = childItems[row];

    // check object
    if (!childItem)
    {
        return {};
    }

    // create index
    return createIndex(row, col, childItem);
}

QModelIndex
GtCoreDatamodel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    // get object from index
    GtObject* childItem = objectFromIndex(index);

    // check object
    if (!childItem)
    {
        return {};
    }

    // get parent object
    GtObject* parentItem = childItem->parentObject();

    // check parent
    if (!parentItem)
    {
        return {};
    }

    // check whether parent object is a session
    if (qobject_cast<GtSession*>(parentItem))
    {
        return {};
    }

    // return index
    return indexFromObject(parentItem, 0);
}

QVariant
GtCoreDatamodel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
        {
            if (index.column() == 0)
            {
                // get object
                GtObject* item = objectFromIndex(index);

                // check object
                if (item)
                {
                    // return object identification string
                    return item->objectName();
                }
            }

            break;
        }

        case UuidRole:
        {
            // get object
            GtObject* item = objectFromIndex(index);

            // check object
            if (item)
            {
                // return object uuid
                return item->uuid();
            }
            break;
        }

        default:
            break;
    }

    return QVariant();
}

bool
GtCoreDatamodel::setData(const QModelIndex& index, const QVariant& value,
                         int role)
{
    // check index
    if (!index.isValid())
    {
        return false;
    }

    // check column
    if (index.column() != 0)
    {
        return false;
    }

    // check role
    if (role != Qt::EditRole)
    {
        return false;
    }

    // get object
    GtObject* item = objectFromIndex(index);

    // check object
    if (!item)
    {
        return false;
    }

    // check renameable flag
    if (!item->isRenamable())
    {
        return false;
    }

    if (value.toString() == item->objectName())
    {
        return false;
    }

    // update identification string
    auto cmd = gtApp->makeCommand(item, item->objectName() + tr(" renamed"));
    Q_UNUSED(cmd)

    item->setObjectName(value.toString());

    return true;
}

QStringList
GtCoreDatamodel::mimeTypes() const
{
    QStringList types;
    types << QStringLiteral("GtObject");
    return types;
}

QMimeData*
GtCoreDatamodel::mimeData(const QModelIndexList& indexes) const
{
    if (!indexes.empty())
    {
        QModelIndex index = indexes.value(0);

        if (index.isValid())
        {
            GtObject* obj = objectFromIndex(index);

            return mimeDataFromObject(obj);
        }
    }

    return new QMimeData();
}

QMimeData*
GtCoreDatamodel::mimeDataFromObject(GtObject* obj, bool newUuid) const
{
    // check object
    if (!obj)
    {
        return NULL;
    }

    // create memento
    GtObjectMemento memento = obj->toMemento(!newUuid);

    QMimeData* mimeData = new QMimeData;

    // append memento to mime data
    mimeData->setData(QStringLiteral("GtObject"), memento.toByteArray());

    // return mime data
    return mimeData;
}

GtObject*
GtCoreDatamodel::objectFromMimeData(const QMimeData* mime, bool newUuid,
                                    GtAbstractObjectFactory* factory)
{
    // check mime data
    if (!mime)
    {
        return nullptr;
    }

    // check mime data format
    if (!mime->hasFormat(QStringLiteral("GtObject")))
    {
        return nullptr;
    }

    // restore memento from mime data
    GtObjectMemento memento(mime->data(QStringLiteral("GtObject")));

    // check memento
    if (memento.isNull())
    {
        return nullptr;
    }

    // check factory. if no factory was set, use default object factory
    if (!factory)
    {
        factory = gtObjectFactory;
    }

    // return object restored from memento
    return memento.restore(factory, newUuid);
}

GtObject*
GtCoreDatamodel::objectFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }

    return static_cast<GtObject*>(index.internalPointer());
}

QModelIndex
GtCoreDatamodel::indexFromObject(GtObject* obj, int col) const
{
    // initialize row
    int row = -1;

    if (qobject_cast<GtProject*>(obj))
    {
        // handle project object
        QList<GtProject*> projects = m_session->projects();

        // get index from project
        row = projects.indexOf(qobject_cast<GtProject*>(obj));
    }
    // handle object
    else if (obj->parent())
    {
        row = obj->childNumber();
    }

    // check row
    if (row < 0)
    {
        return {};
    }

    // create index
    return createIndex(row, col, obj);
}

QModelIndex
GtCoreDatamodel::appendChild(GtObject* child, GtObject* parent)
{
    // check child object
    if (!child)
    {
        gtWarning() << tr("Could not append child!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("child == NULL");
        return {};
    }

    if (!child->factory())
    {
        child->setFactory(gtObjectFactory);
    }

    // check parent object
    if (!parent)
    {
        gtWarning() << tr("Could not append child!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("parent == NULL");
        return {};
    }

    // use multi append function
    QModelIndexList indexList = appendChildren(GtObjectList() << child, parent);

    // check whether child was appended
    if (indexList.isEmpty())
    {
        return {};
    }

    // return model index
    return indexList.first();
}

QModelIndexList
GtCoreDatamodel::appendChildren(QList<GtObject*> const& children,
                                GtObject* parent)
{
    // check parent object
    if (!parent)
    {
        gtWarning() << tr("Could not append children!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("parent == NULL");
        return QModelIndexList();
    }

    // get index of parent object
    QModelIndex parentIndex = indexFromObject(parent);

    // check parent index
    if (!parentIndex.isValid())
    {
        gtWarning() << tr("Could not append children!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("parent index invalid");
        return QModelIndexList();
    }

    // TODO: check whether children can be appended or not before appending

    beginInsertRows(parentIndex, parent->childCount<GtObject*>(),
                    parent->childCount<GtObject*>() + children.size() - 1);

    if (!parent->appendChildren(children))
    {
        return QModelIndexList();
    }

    endInsertRows();

    QModelIndexList retval;

    foreach (GtObject* obj, children)
    {
        retval << indexFromObject(obj);
    }

    // return model index list
    return retval;
}

QModelIndex
GtCoreDatamodel::insertChild(GtObject* child, GtObject* parent, int row)
{
    // check child object
    if (!child)
    {
        gtWarning() << tr("Could not insert child!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("child == NULL");
        return {};
    }

    // check parent object
    if (!parent)
    {
        gtWarning() << tr("Could not insert child!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("parent == NULL");
        return {};
    }

    // get index of parent object
    QModelIndex parentIndex = indexFromObject(parent);

    // check parent index
    if (!parentIndex.isValid())
    {
        gtWarning() << tr("Could not insert children!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("parent index invalid");
        return {};
    }

    // TODO: check whether child can be appended or not before appending

    beginInsertRows(parentIndex, row, row);

    if (!parent->insertChild(row, child))
    {
        return {};
    }

    endInsertRows();

    // return model index
    return indexFromObject(child);
}

bool
GtCoreDatamodel::deleteFromModel(GtObject* obj)
{
    // check object
    if (!obj)
    {
        gtWarning() << tr("Could not delete object!") <<
                    QStringLiteral(" - ") <<
                    QStringLiteral("object == NULL");
        return false;
    }

    // use multi delete function
    return deleteFromModel(QList<GtObject*>() << obj);

}

bool
GtCoreDatamodel::deleteFromModel(QList<GtObject*> objects)
{
    // check object list
    if (objects.isEmpty())
    {
        return true;
    }

    // delete each object separately
    foreach (GtObject* obj, objects)
    {
        // get object model index
        QModelIndex objIndex = indexFromObject(obj);
        // check model index
        if (!objIndex.isValid())
        {
            return false;
        }

        // get parent index
        QModelIndex parentIndex = objIndex.parent();

        // check parent index
        if (!parentIndex.isValid())
        {
            return false;
        }

        beginRemoveRows(parentIndex, obj->childNumber(), obj->childNumber());

        // delete object
        delete obj;

        endRemoveRows();
    }

    // return success state
    return true;
}

QString
GtCoreDatamodel::uniqueObjectName(const QString& name, GtObject* parent)
{
    return gt::makeUniqueName(name, parent);
}

QString
GtCoreDatamodel::uniqueObjectName(const QString& name,
                                  const QModelIndex& parent)
{
    // check model index
    if (!parent.isValid()) return {};

    // check model of index
    if (parent.model() != this) return {};

    // return unique object name
    return uniqueObjectName(name, objectFromIndex(parent));
}

GtObject*
GtCoreDatamodel::objectByPath(const QString& objectPath)
{
    // split object path
    QStringList list = objectPath.split(QStringLiteral(";"));

    // check session
    if (!m_session)
    {
        return nullptr;
    }

    // return object
    return m_session->getObjectByPath(list);
}

GtObject*
GtCoreDatamodel::objectByUuid(const QString& objectUuid)
{
    // check uuid string
    if (objectUuid.isEmpty())
    {
        return nullptr;
    }

    // check session
    if (!m_session)
    {
        return nullptr;
    }

    // return object
    return m_session->getObjectByUuid(objectUuid);
}
