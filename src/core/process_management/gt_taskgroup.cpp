/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2022 by DLR
 *
 *  Created on: 12.10.2022
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDomDocument>
#include <QDomElement>

#include "gt_algorithms.h"
#include "gt_logging.h"
#include "gt_taskgroup.h"
#include "gt_processfactory.h"
#include "gt_task.h"

class GtTaskGroup::Impl
{
public:
    std::reference_wrapper<GtTaskGroup> _pub;

    int _initialized = false;

    Impl (GtTaskGroup& pub) : _pub(pub) { }

    bool updateIndexFile(const QString& projectPath,
                         const GtTaskGroup::SCOPE scope) const;

    QString path(const QString& projectPath,
                 const GtTaskGroup::SCOPE scope) const;

    GtTask* createTaskFromFile(const QString& filePath) const;

    bool saveTaskToFile(GtTask* task, const QString& groupPath) const;

    static bool createSubFolder(QDir& dir,
                         const QString& subFolderId);

    static bool initFileStructure(const QString& projectPath,
                                  const GtTaskGroup::SCOPE scope,
                                  const QString& groupId);

    static bool appendTaskToIndex(const QString& projectPath,
                                  const GtTaskGroup::SCOPE scope,
                                  const QString& groupId,
                                  const QString& taskUuid);

    static QString indexFileName();

    static QString taskFileExtension();

};

GtTaskGroup::GtTaskGroup(const QString& id) :
    m_pimpl{std::make_unique<Impl>(*this)}
{
    setObjectName(id);
}

GtTaskGroup::~GtTaskGroup() = default;

bool
GtTaskGroup::init(const QString& projectPath,
                  const SCOPE scope)
{
    if (m_pimpl-> _initialized)
    {
        gtError() << "task group already initialized!";
        return false;
    }

    QDir dir(m_pimpl->path(projectPath, scope));

    gtInfo() << "task group path = " << dir.absolutePath();

    // read json file
    QFile idxFile(dir.absoluteFilePath(m_pimpl->indexFileName()));

    if (!idxFile.exists())
    {
        // index file not found. empty or new task group
        m_pimpl-> _initialized = true;
        return true;
    }

    // index file found. create tasks from mementos
    if (!idxFile.open(QIODevice::ReadOnly))
    {
        gtError() << "could not open index file!";
        return false;
    }

    QByteArray data = idxFile.readAll();

    QJsonDocument doc(QJsonDocument::fromJson(data));

    QJsonObject json = doc.object();

    // json doc created. close file
    idxFile.close();

    // active tasks
    QJsonArray activeTasks = json[QStringLiteral("active")].toArray();

    for (auto&& e : activeTasks) {
        GtTask* newTask = m_pimpl->createTaskFromFile(
                    dir.absoluteFilePath(e.toString() +
                                         m_pimpl->taskFileExtension()));

        if (newTask)
        {
            gtDebug() << "new task created (" << newTask->uuid() << ")";
            appendChild(newTask);
        }
    }

    m_pimpl-> _initialized = true;
    return true;
}

bool
GtTaskGroup::save(const QString& projectPath,
                  const GtTaskGroup::SCOPE scope) const
{
    if (!m_pimpl-> _initialized)
    {
        gtDebug() << "save procedure not needed. group not initialized! (" <<
                     objectName() << ")";
        return false;
    }

    // init full file structure for given task group
    if (!m_pimpl->initFileStructure(projectPath, scope, objectName()))
    {
        return false;
    }

    // externalize tasks
    foreach (GtTask* task, findDirectChildren<GtTask*>())
    {
        if (!m_pimpl->saveTaskToFile(task, m_pimpl->path(projectPath, scope)))
        {
            return false;
        }
    }

    // final step: update index file
    return m_pimpl->updateIndexFile(projectPath, scope);
}

QString
GtTaskGroup::scopeId(const SCOPE scope)
{
    QString retval;

    switch (scope)
    {
    case GtTaskGroup::USER:
        retval = "_user";
        break;
    case GtTaskGroup::CUSTOM:
        retval = "_custom";
        break;
    default:
        gtError() << "invalid task group scope";
        return {};
    }

    return retval;
}

QString
GtTaskGroup::defaultUserGroupId()
{
    QString retval = qEnvironmentVariable("USER");

    if (retval.isEmpty())
    {
        retval = qEnvironmentVariable("USERNAME");
    }

    return retval;
}

QString
GtTaskGroup::groupPath(const QString& projectPath,
                       const SCOPE scope,
                       const QString& groupId)
{
    QString retval = projectPath + QDir::separator() + "tasks";

    QString scopeId = GtTaskGroup::scopeId(scope);

    if (scopeId.isEmpty())
    {
        gtError() << "invalid task group scope";
        return {};
    }

    retval += QDir::separator() + scopeId + QDir::separator() + groupId;

    return retval;
}

bool
GtTaskGroup::saveTaskElementToFile(const QString& projectPath,
                                   const SCOPE scope,
                                   const QString& groupId,
                                   const QDomElement& taskElement)
{
    QString taskUuid = taskElement.attribute("uuid");

    if (taskUuid.isEmpty())
    {
        gtError() << "task uuid not found. corrupted task element!";
        return false;
    }

    if (!GtTaskGroup::Impl::initFileStructure(projectPath, scope, groupId))
    {
        return false;
    }

    const QString fileName = groupPath(projectPath, scope, groupId) + QDir::separator()
            + taskUuid + GtTaskGroup::Impl::taskFileExtension();

    gtDebug() << "writing task file (" << fileName << ")...";

    QFile taskFile(fileName);

    if (taskFile.exists())
    {
        gtError() << "file already exists!" << taskFile.fileName();
        return false;
    }

    if (!taskFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        gtError() << "Could not open file!" << taskFile.fileName();
        return false;
    }

    QTextStream out(&taskFile);

    QDomDocument doc;

    // element is the QDomElement object
    QString str;
    QTextStream stream(&str, QIODevice::WriteOnly);
    taskElement.save(stream, 2); // stored the content of QDomElement to stream

    doc.setContent(str.toUtf8());

    gtDebug() << "writing doc...";
    out << doc.toString();

    taskFile.close();

    // append task to index
    if (!GtTaskGroup::Impl::appendTaskToIndex(projectPath, scope, groupId,
                                              taskUuid))
    {
        gtError() << "Could not append task to index file!";
        return false;
    }

    return true;
}

bool
GtTaskGroup::Impl::initFileStructure(const QString& projectPath,
                                     const GtTaskGroup::SCOPE scope,
                                     const QString& groupId)
{
    QDir dir(projectPath);

    if (!dir.exists())
    {
        gtError() << "project path not found! (" << projectPath << ")";
        return false;
    }

    if (!createSubFolder(dir, "tasks"))
        return false;

    if (!createSubFolder(dir, GtTaskGroup::scopeId(scope)))
        return false;

    // check group id folder. if not available, create new.
    if (!createSubFolder(dir, groupId))
        return false;

    return true;
}

bool
GtTaskGroup::Impl::appendTaskToIndex(const QString& projectPath,
                                     const SCOPE scope,
                                     const QString& groupId,
                                     const QString& taskUuid)
{
    QDir dir(GtTaskGroup::groupPath(projectPath, scope, groupId));
    const QString _idxFileName = GtTaskGroup::Impl::indexFileName();

    // check for existance
    QFile idxFile(dir.absoluteFilePath(_idxFileName));

    QJsonObject jroot;
    QJsonArray jarray;

    if (idxFile.exists())
    {
        // open index file
        if (!idxFile.open(QIODevice::ReadWrite))
        {
            gtError() << "could not open index file!";
            return false;
        }

        QByteArray data = idxFile.readAll();
        QJsonDocument doc(QJsonDocument::fromJson(data));

        idxFile.resize(0);
        jroot = doc.object();
        jarray = jroot["active"].toArray();
    }
    else
    {
        // open index file
        if (!idxFile.open(QIODevice::WriteOnly))
        {
            gtError() << "could not create index file!";
            return false;
        }
    }

    if (!jarray.contains(taskUuid))
    {
        jarray.push_back(taskUuid);
    }

    jroot.insert(QStringLiteral("active"), jarray);

    QJsonDocument saveDoc(jroot);
    idxFile.write(saveDoc.toJson());

    idxFile.close();

    // we are happy
    return true;
}

bool
GtTaskGroup::Impl::createSubFolder(QDir& dir, const QString& subFolderId)
{
    if (subFolderId.isEmpty())
    {
        gtError() << "folder id is empty!";
        return false;
    }

    if (!dir.cd(subFolderId))
    {
        if (!dir.mkdir(subFolderId))
        {
            gtError() << "could not create folder! (" << subFolderId << ")";
            return false;
        }

        return dir.cd(subFolderId);
    }

    // nothing to do, satisfaction
    return true;
}

bool
GtTaskGroup::Impl::updateIndexFile(const QString &projectPath,
                             const GtTaskGroup::SCOPE scope) const
{
    QDir dir(path(projectPath, scope));
    const QString _idxFileName = indexFileName();

    gtDebug() << "task group index file: " <<
                 dir.absoluteFilePath(_idxFileName);

    // check for existance
    QFile idxFile(dir.absoluteFilePath(_idxFileName));

    // open index file
    if (!idxFile.open(QIODevice::WriteOnly))
    {
        gtError() << "could not create index file!";
        return false;
    }

    QJsonObject jroot;
    QJsonArray jarray;

    // write tasks
    foreach (GtTask* task, _pub.get().findDirectChildren<GtTask*>())
    {
        jarray.push_back(task->uuid());
    }

    jroot.insert(QStringLiteral("active"), jarray);

    QJsonDocument saveDoc(jroot);
    idxFile.write(saveDoc.toJson());

    idxFile.close();

    // we are happy
    return true;
}

QString
GtTaskGroup::Impl::path(const QString& projectPath,
                      const GtTaskGroup::SCOPE scope) const
{
    return _pub.get().groupPath(projectPath, scope, _pub.get().objectName());
}

GtTask*
GtTaskGroup::Impl::createTaskFromFile(const QString& filePath) const
{
    QFile taskFile(filePath);
    GtTask* retval = nullptr;

    if (!taskFile.exists())
    {
        // task file not found
        gtError() << "task file not found (" << filePath << ")";
        return retval;
    }

    // index file found. create tasks from mementos
    QDomDocument document;

    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!document.setContent(&taskFile, true, &errorStr, &errorLine, &errorColumn))
    {
        gtError() << "could not open task file (" << filePath << ")";
        return retval;
    }

    QDomElement root = document.documentElement();

    GtObjectMemento memento(root);
    if (memento.isNull())
    {
        gtError() << "could not parse task file (" << filePath << ")";
        return retval;
    }

    auto obj = memento.restore(gtProcessFactory);

    if (obj)
    {
        retval = qobject_cast<GtTask*>(obj);
        if (!retval)
        {
            gtError() << "invalid task file (" << filePath << ")";
            delete obj;
        }
    }

    return retval;
}

bool
GtTaskGroup::Impl::saveTaskToFile(GtTask* task, const QString& groupPath) const
{
    QFile taskFile(groupPath + QDir::separator() + task->uuid() +
                   taskFileExtension());


    if (!taskFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        gtError() << "Could not open file!" << taskFile.fileName();
        return false;
    }

    QTextStream out(&taskFile);

    QDomDocument doc;
    doc.setContent(task->toMemento().toByteArray());

    out << doc.toString();

    taskFile.close();

    return true;
}

QString
GtTaskGroup::Impl::indexFileName()
{
    return {"index.json"};
}

QString
GtTaskGroup::Impl::taskFileExtension()
{
    return {".gttask"};
}
