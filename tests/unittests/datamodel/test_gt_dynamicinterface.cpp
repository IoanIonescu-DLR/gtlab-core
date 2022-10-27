/* GTlab - Gas Turbine laboratory
 * Source File: test_gt_dynamicinterface.cpp
 * copyright 2022 by DLR
 *
 *  Created on: 01.04.2022
 *  Author: Martin Siggel (AT-TWK)
 *  Tel.: +49 2203 601 2264
 */

#include "gtest/gtest.h"

#include "gt_functional_interface.h"
#include "gt_dynamicinterface.h"

#include "internal/gt_dynamicinterfacehandler.h"

#include <QString>

using gt::interface::makeInterfaceFunction;

class DynamicInterface : public testing::Test
{};

int my_test_sum (int a, int b)
{
    return a + b;
}

QString my_insane_test_fun(const std::string& str1,
                           QString& str2, double v1, int& v2)
{
    return QString("%0,%1,%2,%3").arg(str1.c_str()).arg(str2).arg(v1).arg(v2);
}

TEST_F(DynamicInterface, wrapFunction)
{
    auto itf_fun = gt::interface::makeInterfaceFunction("my_test_sum",
                                                          my_test_sum);
    auto result = itf_fun({1, 2});

    ASSERT_EQ(1, result.size());
    EXPECT_EQ(3, result[0].toInt());

    // too few arguments
    EXPECT_THROW(itf_fun({1}), std::runtime_error);

    // arg 0 cannot be converted into int
    EXPECT_THROW(itf_fun({"bla", 2}), std::runtime_error);
    EXPECT_STREQ("my_test_sum", itf_fun.name().toStdString().c_str());
}

TEST_F(DynamicInterface, wrapQString)
{
    auto mystringfunction = [](QString str) {
        return str;
    };

    auto itf_fun = gt::interface::makeInterfaceFunction("mystringfunction",
                                                          mystringfunction);

    auto result = itf_fun({"Hallo Welt"});

    ASSERT_EQ(1, result.size());
    EXPECT_STREQ("Hallo Welt", result[0].toString().toStdString().c_str());
}

TEST_F(DynamicInterface, wrapStdString)
{
    auto mystringfunction = [](std::string str) {
        return str;
    };

    auto itf_fun = gt::interface::makeInterfaceFunction("mystringfunction2",
                                                          mystringfunction);

    auto result = itf_fun({"Hallo Welt"});

    ASSERT_EQ(1, result.size());
    EXPECT_STREQ("Hallo Welt", result[0].toString().toStdString().c_str());
}

TEST_F(DynamicInterface, wrapLambda)
{
    auto lambda = [](double a, double b) {
        return a*b;
    };

    auto itf_fun = gt::interface::makeInterfaceFunction("my_lambda_mult",
                                                          lambda);

    auto result = itf_fun({3., 4.});

    EXPECT_EQ(1, result.size());
    EXPECT_EQ(12., result[0].toDouble());
}

TEST_F(DynamicInterface, wrapFunctionObject)
{
    struct MyFunctionObj
    {
        explicit MyFunctionObj(double base) : base(base) {}

        double operator()(double head) const
        {
            return base + head;
        }

        double base{0.};
    };

    auto f = MyFunctionObj(10.);

    // sanity check
    ASSERT_EQ(15, f(5.));

    auto itf_fun = gt::interface::makeInterfaceFunction("my_func_obj", f);

    auto result = itf_fun({12.});
    EXPECT_EQ(1, result.size());
    EXPECT_EQ(22., result[0].toDouble());
}

TEST_F(DynamicInterface, returnTuple)
{
    auto itf_fun = gt::interface::makeInterfaceFunction(
        "my_lambda_tuple",
        [](double a, double b) {
            return std::make_tuple(a+b, a-b);
        }
    );

    auto result = itf_fun({10., 4.});
    ASSERT_EQ(2, result.size());
    EXPECT_EQ(14., result[0].toDouble());
    EXPECT_EQ(6., result[1].toDouble());
}


TEST_F(DynamicInterface, returnVoid)
{
    auto itf_fun = gt::interface::makeInterfaceFunction("my_return_void_func",
        [](double) {
            // nothing
        }
    );

    auto result = itf_fun({10.});
    ASSERT_EQ(0, result.size());
}

TEST_F(DynamicInterface, getFunctionFailure)
{
    auto func = gt::interface::getFunction("testmod",
                                            "this_funcion_does_not_exist");
    EXPECT_FALSE(func);
}


TEST_F(DynamicInterface, registerFunctionNoHelp)
{
    ASSERT_TRUE(gt::interface::detail::registerFunction("testmod",
        makeInterfaceFunction("my_test_sum", my_test_sum)));

    auto func = gt::interface::getFunction("testmod", "my_test_sum");
    ASSERT_TRUE(func);

    EXPECT_FALSE(func.help().isEmpty());
}

TEST_F(DynamicInterface, registerFunctionWithHelp)
{
    auto help = "this is the help of my_test_sum2";

    ASSERT_TRUE(gt::interface::detail::registerFunction("testmod",
        makeInterfaceFunction("my_test_sum2", my_test_sum, help)));

    auto func = gt::interface::getFunction("testmod", "my_test_sum2");
    ASSERT_TRUE(func);

    EXPECT_STREQ(help, func.help().toStdString().c_str());
}

TEST_F(DynamicInterface, checkName)
{
    ASSERT_TRUE(gt::interface::detail::registerFunction("testmod",
        makeInterfaceFunction("my_test_sum3", my_test_sum)));

    auto func = gt::interface::getFunction("testmod", "my_test_sum3");
    EXPECT_STREQ("my_test_sum3", func.name().toStdString().c_str());
}

TEST_F(DynamicInterface, passByRef)
{
    ASSERT_TRUE(gt::interface::detail::registerFunction("testmod",
        makeInterfaceFunction("insane_fun", my_insane_test_fun)));

    auto func = gt::interface::getFunction("testmod", "insane_fun");
    auto result = func({"S1", "S2", 3, 4});
    ASSERT_EQ(1, result.size());
    EXPECT_STREQ("S1,S2,3,4", result.at(0).toString().toStdString().c_str());
}

