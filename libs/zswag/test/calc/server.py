import math
import calculator.api as api


def power(request: api.BaseAndExponent):
    response = api.Double(request.base.value**request.exponent.value)
    return response
