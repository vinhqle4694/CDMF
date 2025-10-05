/**
 * @file calculator_service.h
 * @brief Calculator service interface and implementation
 */

#pragma once

#include <cstdint>

namespace cdmf {

/**
 * @brief Calculator service interface
 */
class ICalculatorService {
public:
    virtual ~ICalculatorService() = default;

    /**
     * @brief Add two numbers
     * @param a First number
     * @param b Second number
     * @return Sum of a and b
     */
    virtual int32_t add(int32_t a, int32_t b) = 0;

    /**
     * @brief Subtract two numbers
     * @param a First number
     * @param b Second number
     * @return Difference of a and b
     */
    virtual int32_t subtract(int32_t a, int32_t b) = 0;

    /**
     * @brief Multiply two numbers
     * @param a First number
     * @param b Second number
     * @return Product of a and b
     */
    virtual int32_t multiply(int32_t a, int32_t b) = 0;

    /**
     * @brief Divide two numbers
     * @param a First number (dividend)
     * @param b Second number (divisor)
     * @return Quotient of a divided by b
     * @throws std::invalid_argument if b is zero
     */
    virtual int32_t divide(int32_t a, int32_t b) = 0;
};

namespace services {

/**
 * @brief Calculator service implementation
 */
class CalculatorService : public ICalculatorService {
public:
    CalculatorService() = default;
    ~CalculatorService() override = default;

    int32_t add(int32_t a, int32_t b) override;
    int32_t subtract(int32_t a, int32_t b) override;
    int32_t multiply(int32_t a, int32_t b) override;
    int32_t divide(int32_t a, int32_t b) override;
};

} // namespace services
} // namespace cdmf
