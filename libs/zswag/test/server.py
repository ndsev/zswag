import math
import calculator.api as api


def powerOfTwo(request):
    response = api.U64.fromFields(request.getValue()**2)
    return response


def squareRoot(request):
    response = api.Double.fromFields(math.sqrt(request.getValue()))
    return response
