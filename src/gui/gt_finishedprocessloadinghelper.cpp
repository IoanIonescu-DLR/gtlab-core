
#include <QFile>
#include <QDir>

#include "gt_objectmementodiff.h"
#include "gt_application.h"
#include "gt_finishedprocessloadinghelper.h"

GtFinishedProcessLoadingHelper::GtFinishedProcessLoadingHelper(
        QList<GtObjectMemento>* data,
        QPointer<GtObject> source,
        const QString& taskName) :
    m_data(data),
    m_sumDiff(std::make_unique<GtObjectMementoDiff>()),
    m_source(source),
    m_taskName(taskName)
{

}

GtFinishedProcessLoadingHelper::~GtFinishedProcessLoadingHelper() = default;

void
GtFinishedProcessLoadingHelper::run()
{
    for (int i = 0; i < m_data->size(); ++i)
    {
        GtObjectMemento memento = m_data->at(i);

        if (GtObject* target = m_source->getObjectByUuid(memento.uuid()))
        {
            GtObjectMemento old = target->toMemento(true);
            GtObjectMementoDiff diff(old, memento);

            *m_sumDiff << diff;
        }
    }
}

GtObjectMementoDiff*
GtFinishedProcessLoadingHelper::sumDiff() const
{
    return m_sumDiff.get();
}
