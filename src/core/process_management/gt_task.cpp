/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2015 by DLR
 *
 *  Created on: 28.07.2015
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include "gt_task.h"
#include "gt_accessdata.h"
#include "gt_calculator.h"
#include "gt_abstractrunnable.h"
#include "gt_coreapplication.h"
#include "gt_objectlinkproperty.h"
#include "gt_objectpathproperty.h"
#include "gt_processrunnerglobals.h"

#include <QDebug>
#include <QThreadPool>

#include <algorithm>

struct GtTask::Impl
{
    /// Event loop
    QEventLoop eventLoop;

    /// List of all data to merge
    QList<GtObjectMemento> dataToMerge;

    /// Monitoring data table
    GtMonitoringDataTable monitoringDataTable;

    /// Interruption flag
    QAtomicInt interrupt;

    /// Access Selection property for the process runner
    GtAccessSelectionProperty processRunner{
        "processRunner", tr("Process Runner"),
        gt::process_runner::S_ACCESS_ID,
        tr("Process Runner to run task with. Only relevant for the root task")
    };
};

GtTask::GtTask() :
    m_maxIter(QStringLiteral("maxIter"), tr("Number Of Iterations"),
              tr("Number of iteration steps"), GtUnit::Category::None,
              GtIntProperty::BoundLow, 1, 1),
    m_currentIter(QStringLiteral("currentIter"), tr("Current Iteration")),
    m_lastEval(GtTask::EVAL_FINISHED),
    pimpl(std::make_unique<Impl>())
{
    setObjectName(QStringLiteral("Task"));

    connect(this, &GtTask::finished,
            &pimpl->eventLoop, &QEventLoop::quit);

    setState(GtTask::NONE);
    setFlag(GtObject::UserRenamable, true);

    m_currentIter.setVal(0);

    qRegisterMetaType<GtMonitoringDataSet>("GtMonitoringDataSet");

    registerProperty(pimpl->processRunner, tr("Execution"));


    pimpl->processRunner.hide(!gtApp || !gtApp->devMode());
}

GtTask::~GtTask() = default;

bool
GtTask::exec()
{
    setRunnable(nullptr);

    // check skipped indicator
    if (isSkipped())
    {
        setState(GtTask::SKIPPED);
        gtDebug() << objectName() << tr(" skipped");
        return true;
    }

    setRunnable(findParent<GtAbstractRunnable*>());

    // collect all calculator properties
    QList<GtAbstractProperty*> props = fullPropertyList();

    // search for object link and object path properties
    foreach (GtAbstractProperty* prop, props)
    {
        if (GtObjectLinkProperty* objLink =
                    qobject_cast<GtObjectLinkProperty*>(prop))
        {
            // object link property found
            GtObject* linkedObj =
                runnable()->data<GtObject*>(objLink->linkedObjectUUID());

            if (linkedObj)
            {
                // linked object found -> store inside list
                linkedObjects().append(linkedObj);
            }
            else
            {
                qDebug() << "Linked object not found in runnable";
            }
        }
        else if (GtObjectPathProperty* objPath =
                     qobject_cast<GtObjectPathProperty*>(prop))
        {
            // object path property found
            GtObject* linkedObj =
                runnable()->data<GtObject*>(objPath->path());

            if (linkedObj)
            {
                // linked object found -> store inside list
                linkedObjects().append(linkedObj);
            }
            else
            {
                qDebug() << "Linked object not found in runnable";
            }
        }
    }

    // reset evaluator variables
    m_currentIter.setVal(0);

    // Initialize individual evaluator setting
    if (!setUp())
    {
        setState(GtProcessComponent::FAILED);
        return false;
    }

    // check max. iteration steps
    if (m_maxIter <= 0)
    {
        setState(GtProcessComponent::FAILED);
        return false;
    }

    setState(GtProcessComponent::RUNNING);

    // clear existing monitoring data
    emit triggerClearMonitoringData();

    // start iteration
    if (!runIteration())
    {
        setState(GtProcessComponent::FAILED);
        return false;
    }

    // max. number of iteration steps reached
    if (childHasWarnings())
    {
        setState(GtProcessComponent::WARN_FINISHED);
    }
    else if (this->currentState() == GtProcessComponent::WARN_FINISHED)
    {
        /// nothing to do ==> state is already set
    }
    else
    {
        setState(GtProcessComponent::FINISHED);
    }

    // return success
    return true;
}

void
GtTask::run(GtAbstractRunnable* r)
{
    setState(GtTask::RUNNING);
    pimpl->dataToMerge.clear();

    setRunnable(r);

    QThreadPool* tp = QThreadPool::globalInstance();

    if (!tp)
    {
        return;
    }

    runnable()->setAutoDelete(false);

    connect(runnable().data(), &GtAbstractRunnable::runnableFinished,
            this, &GtTask::handleRunnableFinished);

    tp->start(runnable());

    qDebug() << "#### exec event loop...";

    pimpl->eventLoop.exec();

    qDebug() << "#### exec finished";
}

QList<GtCalculator*>
GtTask::calculators()
{
    return findDirectChildren<GtCalculator*>();
}

QList<GtProcessComponent*>
GtTask::processComponents()
{
    return findDirectChildren<GtProcessComponent*>();
}

QList<GtObjectMemento>&
GtTask::dataToMerge()
{
    return pimpl->dataToMerge;
}

bool
GtTask::setUp()
{
    // nothing to do here
    return true;
}

GtTask::EVALUATION
GtTask::evaluate()
{
    // nothing to do here
    return GtTask::EVAL_OK;
}

bool
GtTask::runIteration()
{
    // get list of all child process components
    //    QList<GtProcessComponent*> childs = processComponents();

    qDebug() << "starting iteration...";

    do
    {
        if (!runChildElements())
        {
            return false;
        }

        // check evaluation result
        switch (m_lastEval)
        {
            case GtTask::EVAL_FINISHED:
            {
                // iteration finished successfully
                setState(GtProcessComponent::FINISHED);
                return true;
            }

            case GtTask::EVAL_FAILED:
            {
                // iteration failed
                setState(GtProcessComponent::FAILED);
                return false;
            }

            default:
                break;
        }

    }
    while (m_currentIter < m_maxIter);

    return true;
}

int
GtTask::monitoringDataSize() const
{
    return pimpl->monitoringDataTable.size();
}

const GtMonitoringDataTable&
GtTask::monitoringDataTable()
{
    return pimpl->monitoringDataTable;
}

int
GtTask::maxIterationSteps() const
{
    return m_maxIter;
}

int
GtTask::currentIterationStep() const
{
    return m_currentIter.getVal();
}

GtAccessData
GtTask::selectedProcessRunner() const
{
    return pimpl->processRunner.accessData();
}

void
GtTask::onObjectDataMerged()
{
    foreach (GtPropertyConnection* connection,
             findDirectChildren<GtPropertyConnection*>())
    {
        if (!connection->isConnected())
        {
            connection->makeConnection();
        }
    }
}

void
GtTask::enableMaxIterationProperty()
{
    registerProperty(m_maxIter);
}

void
GtTask::enableCurrentIterationMonitoring()
{
    registerMonitoringProperty(m_currentIter);
}

bool
GtTask::runChildElements()
{
    QList<GtProcessComponent*> childs = processComponents();

    // increment current iteration step and continue iteration
    m_currentIter.setVal(m_currentIter.getVal() + 1);

    qDebug() << "iteration step (" << m_currentIter << "/" << m_maxIter <<
             ")";

    // trigger transfer of monitoring properties before running calculators
    emit transferMonitoringProperties();

    // reset state of child prcess elements
    foreach (GtProcessComponent* comp, childs)
    {
        comp->setStateRecursively(GtProcessComponent::QUEUED);
    }

    qDebug() << "running calculators...";

    // run calculators
    foreach (GtProcessComponent* comp, childs)
    {
        if (!comp->exec())
        {
            // calculator run failed
            setState(GtProcessComponent::FAILED);

            qDebug() << "   |-> run failed!";

            return false;
        }

        if (isInterruptionRequested())
        {
            gtWarning() << "task terminated!";
            setState(GtProcessComponent::TERMINATED);
            return false;
        }

        GtCalculator* calc = qobject_cast<GtCalculator*>(comp);

        if (calc && calc->runFailsOnWarning())
        {
            if (calc->currentState() == GtProcessComponent::WARN_FINISHED)
            {
                calc->setState(FAILED);
                setState(GtProcessComponent::FAILED);
                return false;
            }
        }
    }

    qDebug() << "evaluating...";
    // evaluate current iteration step
    m_lastEval = evaluate();

    // trigger transfer of monitoring properties after evaluation
    emit transferMonitoringProperties();

    // collect monitoring data for entire task
    GtMonitoringDataSet monData = collectMonitoringData();

    // check whether monitoring data has entries
    if (!monData.isEmpty())
    {
        // monitoring data available - emit signal
        emit monitoringDataTransfer(m_currentIter, monData);
    }

    return true;
}

GtMonitoringDataSet
GtTask::collectMonitoringData()
{
    GtMonitoringDataSet retval;

    collectMonitoringDataHelper(retval, this);

    return retval;
}

QList<GtPropertyConnection*>
GtTask::collectPropertyConnections()
{
    QList<GtPropertyConnection*> retval;

    collectPropertyConnectionHelper(retval, this);

    return retval;
}

void
GtTask::requestInterruption()
{
    pimpl->interrupt.testAndSetOrdered(0, 1);
    setState(TERMINATION_REQUESTED);
}

bool
GtTask::isInterruptionRequested() const
{
    return static_cast<int>(pimpl->interrupt);
}

void
GtTask::collectMonitoringDataHelper(GtMonitoringDataSet& map,
                                    GtProcessComponent* component)
{
    // check component
    if (!component)
    {
        return;
    }

    // get monitoring properties
    QList<GtAbstractProperty*> monProps = component->monitoringProperties();

    // check whether monitoring properties exists
    if (!monProps.isEmpty())
    {
        // create new monitoring data container
        GtMonitoringData monData;

        // iterate over monitoring properties and append them to container
        foreach (GtAbstractProperty* prop, monProps)
        {
            monData.addData(prop->ident(), prop->valueToVariant());
        }

        // append monitoring data container to monitoring map
        map.insert(component->uuid(), monData);
    }

    // iterate over children
    foreach (GtProcessComponent* child,
             component->findDirectChildren<GtProcessComponent*>())
    {
        // collect data for each child recursively
        collectMonitoringDataHelper(map, child);
    }
}

void
GtTask::collectPropertyConnectionHelper(QList<GtPropertyConnection*>& list,
                                        GtProcessComponent* component)
{
    // check component
    if (!component)
    {
        return;
    }

    foreach (GtPropertyConnection* connection,
             findDirectChildren<GtPropertyConnection*>())
    {
        // check whether same connection already exists in list
        if (!list.contains(connection))
        {
            list << connection;
        }
    }

    // iterate over children
    foreach (GtProcessComponent* child,
             component->findDirectChildren<GtProcessComponent*>())
    {
        // collect data for each child recursively
        collectPropertyConnectionHelper(list, child);
    }
}

bool
GtTask::childHasWarnings() const
{
    auto const childs = findDirectChildren<GtProcessComponent*>();
    return std::any_of(std::begin(childs), std::end(childs),
                       [](const GtProcessComponent* child) {
        return child->currentState() == WARN_FINISHED;
    });
}

void
GtTask::handleRunnableFinished()
{
    bool success = true;

    qDebug() << __FUNCTION__;

    if (!runnable()->successful())
    {
        success = false;
    }
    else
    {
        pimpl->dataToMerge.append(runnable()->outputData());
    }

    disconnect(runnable().data(), &GtAbstractRunnable::runnableFinished,
               this, &GtTask::handleRunnableFinished);

    delete runnable();

    if (success)
    {
        if (childHasWarnings())
        {
            setState(GtProcessComponent::WARN_FINISHED);
        }
        else
        {
            setState(GtProcessComponent::FINISHED);
        }
    }
    else
    {
        setState(GtProcessComponent::FAILED);
    }

    emit finished();
}

void
GtTask::onMonitoringDataAvailable(int iteration, GtMonitoringDataSet const& set)
{
    // append data set to data table and check success
    if (!pimpl->monitoringDataTable.append(iteration, set))
    {
        gtWarning().medium() << tr("Could not append data set!");
        return;
    }

    emit monitoringDataAvailable();
}

void
GtTask::clearMonitoringData()
{
    pimpl->monitoringDataTable.clear();
}
