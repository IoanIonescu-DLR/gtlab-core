/* GTlab - Gas Turbine laboratory
 * copyright 2009-2022 by DLR
 *
 *  Created on: 9.9.2022
 *  Author: Marius Bröcker (AT-TWK)
 *  E-Mail: marius.broecker@dlr.de
 */

#ifndef TESTPORCESSSTATESCALCULATOR_H
#define TESTPORCESSSTATESCALCULATOR_H

#include "gt_calculator.h"
#include "gt_doubleproperty.h"
#include "gt_intproperty.h"

/**
 * @brief The TestProcessStatesCalculator class
 * Changes the states of the calculator to test to test the graphical
 * represetatin
 */
class TestProcessStatesCalculator : public GtCalculator
{
    Q_OBJECT

public:

    Q_INVOKABLE TestProcessStatesCalculator();

    bool run() override;

private:

    /// Defines the time to sleep until the next state is set
    GtDoubleProperty m_pInterval;

    /// How often should the states change
    GtIntProperty m_pIterations;
};

#endif // TESTPORCESSSTATESCALCULATOR_H
