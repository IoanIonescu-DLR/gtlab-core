/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2015 by DLR
 *
 *  Created on: 08.10.2015
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include <QIcon>
#include <QFont>

#include "gt_styledlogmodel.h"
#include "gt_logmodel.h"
#include "gt_application.h"
#include "gt_palette.h"

#include "QsLogLevel.h"

GtStyledLogModel::GtStyledLogModel(QObject* parent) :
    QIdentityProxyModel(parent)
{

}

QVariant
GtStyledLogModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= gtLogModel->rowCount())
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DecorationRole:
        {
            const int level =
                gtLogModel->data(index,
                                 GtLogModel::LogLevelRole).toInt();

            switch (level)
            {
                case QsLogging::DebugLevel:
                    return gtApp->icon("bugIcon.png");

                case QsLogging::InfoLevel:
                    return gtApp->icon("infoBlueIcon_16.png");

                case QsLogging::WarnLevel:
                    return gtApp->icon("processFailedIcon_16.png");

                case QsLogging::ErrorLevel:
                    return gtApp->icon("errorIcon_16.png");

                case QsLogging::FatalLevel:
                    return gtApp->icon("fatalIcon_16.png");
            }
        }

        case Qt::TextColorRole:
        {
            const int level =
                gtLogModel->data(index,
                                 GtLogModel::LogLevelRole).toInt();
            bool darkMode = gtApp->inDarkMode();
            switch (level)
            {
                case QsLogging::WarnLevel:
                    return GtPalette::loggingWarnTextColor(darkMode);

                case QsLogging::ErrorLevel:
                    return GtPalette::loggingErrorTextColor(darkMode);

                case QsLogging::FatalLevel:
                    return GtPalette::loggingFatalTextColor(darkMode);
            }
        }

        case Qt::FontRole:
        {
            const int level =
                gtLogModel->data(index,
                                 GtLogModel::LogLevelRole).toInt();

            switch (level)
            {
                case QsLogging::DebugLevel:
                {
                    QFont font =
                        QIdentityProxyModel::data(index, role).value<QFont>();
                    font.setItalic(true);
                    return font;
                }

                case QsLogging::ErrorLevel:
                {
                    QFont font =
                        QIdentityProxyModel::data(index, role).value<QFont>();
                    font.setBold(true);
                    return font;
                }
            }
        }

        case Qt::BackgroundColorRole:
        {
            const int level =
                gtLogModel->data(index, GtLogModel::LogLevelRole).toInt();

            bool darkMode = gtApp->inDarkMode();

            switch (level)
            {
                case QsLogging::FatalLevel:
                    return GtPalette::loggingFatalBackgroundColor(darkMode);
            }
        }
    }

    return QIdentityProxyModel::data(index, role);
}

