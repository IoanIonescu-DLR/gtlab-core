/* GTlab - Gas Turbine laboratory
 * Source File: gt_processcomponent.h
 * copyright 2009-2016 by DLR
 *
 *  Created on: 29.01.2016
 *      Author: Carsten Klein (AT-TWK)
 *		  Tel.: +49 2203 601 2859
 */

#ifndef GTPROCESSCOMPONENT_H
#define GTPROCESSCOMPONENT_H

#include "gt_core_exports.h"
#include "gt_object.h"
#include "gt_boolproperty.h"
#include "gt_objectpath.h"

class QDir;
class GtAbstractRunnable;
class GtTask;
class GtObjectLinkProperty;
class GtObjectPathProperty;

/**
 * @brief The GtProcessComponent class
 */
class GT_CORE_EXPORT GtProcessComponent : public GtObject
{
    Q_OBJECT

public:
    enum STATE
    {
        NONE = 0,
        RUNNING, // 1
        QUEUED, // 2
        FAILED, // 3
        FINISHED, // 4
        WARN_FINISHED, // 5
        CONNECTING, // 6
        SKIPPED, // 7
        TERMINATION_REQUESTED, // 8
        TERMINATED // 9
    };

    /**
     * @brief Main execution method of the process component.
     * @return Whether execution procedure was successful or not.
     */
    virtual bool exec() = 0;

    /**
     * @brief Returns current state of the process component.
     * @return Current state of process component.
     */
    GtProcessComponent::STATE currentState() const;

    /**
     * @brief Returns true if the process component is set to skip mode.
     * @return Skip indicator.
     */
    bool isSkipped() const;

    /**
     * @brief Sets the process component skip mode.
     * @param Whether the component should be skipped or not.
     */
    void setSkipped(bool val);

    /**
     * @brief Sets current state of process component. Is always set to SKIPPED
     * if skipped indicator is active.
     * @param New state.
     */
    void setState(GtProcessComponent::STATE state);

    /**
     * @brief Sets current state of process component and all his child
     * elements recursively.
     * @param New state.
     */
    void setStateRecursively(GtProcessComponent::STATE state);

    /**
     * @brief Returns list of all monitoring properties.
     * @return List of monitoring properties.
     */
    const QList<GtAbstractProperty*>& monitoringProperties();

    /**
     * @brief Returns list of all read and write properties.
     * @return List of read and write properties.
     */
    QList<GtAbstractProperty*> readWriteProperties();

    /**
     * @brief Returns list of all non read only properties.
     * @return List of non read only properties.
     */
    QList<GtAbstractProperty*> nonReadOnlyProperties();

    /**
     * @brief Resets all monitoring properties recursively for whole process
     * component tree.
     */
    void resetMonitoringProperties();

    /**
     * @brief Returns const pointer to root task of current process component.
     *  Null pointer will be returned if no root task was found.
     * @return Root task.
     */
    const GtTask* rootTask() const;

    /**
     * @brief Returns pointer to root task of current process component.
     *  Null pointer will be returned if no root task was found.
     * @return Root task.
     */
    GtTask* rootTask();

    /**
     * @brief isReady
     * @return
     */
    bool isReady() const;

    /**
     * @brief Returns state of warning flag.
     * @return Warning flag.
     */
    bool hasWarnings() const;

    /**
     * @brief Returns process and project specific temporary directory.
     * @return Temporary directory object.
     */
    QDir tempDir();

    /**
     * @brief Returns datamodel object based on given object link property.
     * If no object is found nullpointer is returned.
     * @tparam T Object type
     * @param prop Object link property
     * @return Object corresponding to given object link property
     */
    template <class T = GtObject*>
    T data(GtObjectLinkProperty& prop)
    {
        const QString uuid = dataHelper(prop);
        return data<T>(uuid);
    }

    /**
     * @brief Returns datamodel object based on given object path property.
     * If no object is found nullpointer is returned.
     * @tparam T Object type
     * @param prop Object path property
     * @return Object corresponding to given object path property
     */
    template <class T = GtObject*>
    T data(GtObjectPathProperty& prop)
    {
        const GtObjectPath path = pathHelper(prop);
        return data<T>(path);
    }

    /**
     * @brief Returns datamodel object based on given object uuid. If no
     * object is found nullpointer is returned.
     * @tparam T Object type
     * @param uuid Object uuid string
     * @return Object corresponding to given uuid
     */
    template <class T = GtObject*>
    T data(const QString& uuid)
    {
        foreach (QPointer<GtObject> p, m_linkedObjects)
        {
            if (!p)
            {
                return nullptr;
            }

            GtObject* obj = p.data();

            if (!obj)
            {
                continue;
            }

            if (obj->uuid() == uuid)
            {
                return qobject_cast<T>(obj);
            }
        }

        gtInfo() << "obj not found - uuid =" << uuid;

        return nullptr;
    }

    /**
     * @brief Returns datamodel object based on given object path. If no
     * object is found nullpointer is returned.
     * @tparam T Object type
     * @param path Object path
     * @return Object corresponding to given object path
     */
    template <class T = GtObject*>
    T data(const GtObjectPath& path)
    {
        foreach (QPointer<GtObject> p, m_linkedObjects)
        {
            GtObject* obj = p.data();

            if (!obj)
            {
                continue;
            }

            if (obj->objectPath() == path.toString())
            {
                return qobject_cast<T>(obj);
            }
        }

        gtInfo() << "obj not found - path =" << path.toString();

        return nullptr;
    }

public slots:
    /**
     * @brief Handles process component state changes.
     * @param New state.
     */
    void handleStateChanged(GtProcessComponent::STATE state);

protected:
    /**
     * @brief Constructor
     */
    GtProcessComponent();

    /**
     * @brief registerMonitoringProperty
     * @param property
     */
    void registerMonitoringProperty(GtAbstractProperty& property);

    /**
     * @brief Sets warning flag.
     * @param val New warning flag state.
     */
    void setWarningFlag(bool val);

    /**
     * @brief Returns uuid string of given object link property.
     * @param prop Object link property.
     * @return Uuid string.
     */
    QString dataHelper(GtObjectLinkProperty& prop) const;

    /**
     * @brief Returns object path of given object path property.
     * @param prop Object path property.
     * @return Object path.
     */
    GtObjectPath pathHelper(GtObjectPathProperty& prop) const;

    /**
     * @brief Returns string value of environment variable corresponding to
     * given identification string.
     * @param var Environment variable identification string
     * @return String value of environment variable
     */
    QString environmentVariable(const QString& var) const;

    /**
     * @brief Returns path of current project.
     * Note: only possible when calculator is already running.
     * @return Pasth of current project.
     */
    QString projectPath() const;

    /// Runnable pointer
    QPointer<GtAbstractRunnable> m_runnable;

    /// Path to process/project specific temporary path.
    QString m_tempPath;

    /// List of linked datamodel objects.
    QList<QPointer<GtObject>> m_linkedObjects;
private:
    /// Current process component state
    GtProcessComponent::STATE m_state;

    /// Monitoring properties
    QList<GtAbstractProperty*> m_monitorProperties;

    /// Skip indicator
    GtBoolProperty m_skipped;

    /// Warning flag
    bool m_warning;

signals:
    /**
     * @brief Emitted if skip property changed.
     */
    void skipPropertyChanged();

    /**
     * @brief Emitted on state change.
     */
    void stateChanged(GtProcessComponent::STATE);

    /**
     * @brief Emitted to transfer monitoring properties.
     */
    void transferMonitoringProperties();

};

Q_DECLARE_METATYPE(GtProcessComponent::STATE);

#endif // GTPROCESSCOMPONENT_H
