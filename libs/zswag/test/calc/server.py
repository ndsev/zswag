import math
import calculator.api as api
from functools import reduce


def power(request: api.BaseAndExponent):
    response = api.Double(request.base.value**request.exponent.value)
    return response


def int_sum(request: api.Integers):
    return api.Double(sum(request.values))


def byte_sum(request):
    return api.Double(sum(request.values))


def int_mul(request):
    return api.Double(reduce(int.__mul__, request.values))


def float_mul(request):
    return api.Double(reduce(float.__mul__, request.values))


def bit_mul(request):
    return api.Bool(reduce(bool.__and__, request.values))


def identity(request):
    return request

def concat(request):
    return api.String(reduce(str.__add__, request.values))

def name(request):
    return api.String(request.value.name)
