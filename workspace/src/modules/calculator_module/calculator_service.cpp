/**
 * @file calculator_service.cpp
 * @brief Calculator service implementation
 */

#include "calculator_service.h"
#include "utils/log.h"
#include <stdexcept>

namespace cdmf {
namespace services {

int32_t CalculatorService::add(int32_t a, int32_t b) {
    int32_t result = a + b;
    LOGI_FMT("Calculator: " << a << " + " << b << " = " << result);
    return result;
}

int32_t CalculatorService::subtract(int32_t a, int32_t b) {
    int32_t result = a - b;
    LOGI_FMT("Calculator: " << a << " - " << b << " = " << result);
    return result;
}

int32_t CalculatorService::multiply(int32_t a, int32_t b) {
    int32_t result = a * b;
    LOGI_FMT("Calculator: " << a << " * " << b << " = " << result);
    return result;
}

int32_t CalculatorService::divide(int32_t a, int32_t b) {
    if (b == 0) {
        LOGE("Calculator: Division by zero attempted");
        throw std::invalid_argument("Division by zero");
    }
    int32_t result = a / b;
    LOGI_FMT("Calculator: " << a << " / " << b << " = " << result);
    return result;
}

} // namespace services
} // namespace cdmf
