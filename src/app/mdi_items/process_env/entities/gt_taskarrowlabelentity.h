/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2016 by DLR
 *
 *  Created on: 12.01.2016
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#ifndef GTTASKARROWLABELENTITY_H
#define GTTASKARROWLABELENTITY_H

#include <QGraphicsTextItem>

class GtObject;

/**
 * @brief The GtTaskArrowLabelEntity class
 */
class GtTaskArrowLabelEntity : public QGraphicsObject
{
    Q_OBJECT

public:
    /**
     * @brief GtTaskArrowLabelEntity
     * @param parent
     */
    explicit GtTaskArrowLabelEntity(QGraphicsItem* parent = nullptr);

    /**
     * @brief paint
     * @param painter
     * @param option
     * @param widget
     */
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    /**
    * @brief boundingRect
    * @return
    */
    virtual QRectF boundingRect() const override;

protected:
    /**
     * @brief dragEnterEvent
     * @param event
     */
    virtual void dragEnterEvent(
            QGraphicsSceneDragDropEvent* event) override;

    /**
     * @brief dragLeaveEvent
     * @param event
     */
    virtual void dragLeaveEvent(
            QGraphicsSceneDragDropEvent* event) override;

    /**
     * @brief dropEvent
     * @param event
     */
    virtual void dropEvent(
            QGraphicsSceneDragDropEvent* event) override;

signals:
    void addNewItem(GtObject* obj);

};

#endif // GTTASKARROWLABELENTITY_H
