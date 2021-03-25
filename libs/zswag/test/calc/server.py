import math
import calculator.api as api
from functools import reduce


def power(request: api.BaseAndExponent):
    response = api.Double(request.base.value**request.exponent.value)
    return response


def isum(request: api.Integers):
    return api.Double(sum(request.values))


def imul(request):
    return api.Double(reduce(int.__mul__, request.values))


def bsum(request):
    return api.Double(sum(request.values))


def identity(request):
    return request
