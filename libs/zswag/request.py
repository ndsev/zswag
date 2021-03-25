import inspect
import base64
import zserio
import struct
import functools
from typing import Type, Tuple, Any, Dict, Union, get_args, get_origin, List
from pyzswagcl import \
    OpenApiConfigMethod, \
    OpenApiConfigParam, \
    OpenApiConfigParamFormat, \
    ZSERIO_REQUEST_PART_WHOLE

NoneType = type(None)

# TODO: Booleans?
# Table here: https://docs.python.org/3/library/struct.html#format-characters
C_STRUCT_LITERAL_PER_TYPE_AND_SIZE: Dict[Tuple[type, int], str] = {
    (bool, 1): "!b",
    (bool, 2): "!h",
    (bool, 4): "!i",
    (bool, 8): "!q",
    (int, 1): "!b",
    (int, 2): "!h",
    (int, 4): "!i",
    (int, 8): "!q",
    (float, 4): "!f",
    (float, 8): "!d",
}


# Recursive setattr function, adopted from SO:
#  https://stackoverflow.com/questions/31174295/getattr-and-setattr-on-nested-subobjects-chained-properties
def rsetattr(obj, attr, val):
    pre, _, post = attr.rpartition('.')
    return setattr(rgetattr(obj, pre) if pre else obj, post, val)


def rgetattr(obj, attr, *args):
    def _getattr(obj, attr):
        return getattr(obj, attr, *args)
    return functools.reduce(_getattr, [obj] + attr.split('.'))


# Returns a class instance and a dictionary which reveals
# recursive type information for array/built-in-typed zserio struct members.
def make_instance_and_typeinfo(t: Type, field_name_prefix="") -> Tuple[Any, Dict[str, Any]]:
    result_instance: t = t()
    result_member_types: Dict[str, Any] = {}
    # Zserio initializes nested child structs with None if they are not
    # specified in the constructor. This is a problem, since request
    # parameters may refer to deeply nested fields, so the nested objects
    # must always exist. We infer the nested object types from the constructor-
    # params: Nested object init params are typed as Union[Nested, None]. We then
    # instantiate the child object's type and assign it to the parent member.
    for arg_name, arg_type in result_instance.__init__.__func__.__annotations__.items():
        if arg_name.endswith("_"):
            field_name: str = arg_name.strip('_')
            if get_origin(arg_type) is Union:
                union_args = get_args(arg_type)
                if len(union_args) == 2 and inspect.isclass(union_args[0]) and union_args[1] is NoneType:
                    instance, member_types = make_instance_and_typeinfo(
                        union_args[0], field_name_prefix+field_name+".")
                    setattr(result_instance, f"_{field_name}_", instance)
                    result_member_types.update(member_types)
                    continue
            elif get_origin(arg_type) is list:
                arg_type = [get_args(arg_type)[0]]
            result_member_types[field_name_prefix+field_name] = arg_type
    return result_instance, result_member_types


# Get a byte buffer from a string which is encoded in a given format
def str_to_bytes(s: str, fmt: OpenApiConfigParamFormat) -> bytes:
    if fmt == OpenApiConfigParamFormat.BASE64:
        return base64.b64decode(s)
    elif fmt == OpenApiConfigParamFormat.BASE64URL:
        return base64.urlsafe_b64decode(s)
    elif fmt == OpenApiConfigParamFormat.HEX:
        return bytes.fromhex(s)
    else:  # if fmt in (OpenApiConfigParamFormat.BINARY, OpenApiConfigParamFormat.STRING):
        return bytes(s, encoding="raw_unicode_escape")


# Convert a single passed parameter value to it's correct type
def parse_param_value(param: OpenApiConfigParam, target_type: Type, value: str) -> Any:
    if param.format == OpenApiConfigParamFormat.STRING:
        if target_type is bool:
            return bool(int(value))
        return target_type(value)
    if param.format == OpenApiConfigParamFormat.HEX and target_type is int:
        return int(value, 16)
    value_as_bytes = str_to_bytes(value, param.format)
    struct_literal_key = (target_type, len(value_as_bytes))
    if struct_literal_key in C_STRUCT_LITERAL_PER_TYPE_AND_SIZE:
        return target_type(struct.unpack(
            C_STRUCT_LITERAL_PER_TYPE_AND_SIZE[struct_literal_key],
            value_as_bytes)[0])
    elif target_type is str:
        return str(value_as_bytes, 'utf-8')
    raise RuntimeError(f"Cannot convert {len(value_as_bytes)} to {target_type}")


# Convert an array of passed parameter values to their correct type
def parse_param_values(param: OpenApiConfigParam, target_type: Type, value: List[str]) -> List[Any]:
    return [parse_param_value(param, target_type, item) for item in value]


# Get a blob for a zserio request type, a set of request parameter values
# and an OpenAPI method path spec.
def request_object_blob(*, req_t: Type, spec: OpenApiConfigMethod, **kwargs) -> bytes:
    # Lazy instantiation of request object and type info
    req: Union[req_t, None] = None
    req_field_types: Dict = {}

    # Apply parameters
    param_name: str
    param: OpenApiConfigParam
    for param_name, param in spec.parameters.items():
        # Get raw string value
        value: Union[str, List[str]] = param.default_value
        if param_name in kwargs:
            value = kwargs[param_name]
        # Convert string value to whole blob
        if param.field == ZSERIO_REQUEST_PART_WHOLE:
            return str_to_bytes(value, param.format)
        # First non-whole request parameter: Synthetic request object
        if not req:
            req, req_field_types = make_instance_and_typeinfo(req_t)
        # Convert string value to correct type
        target_type = req_field_types[param.field]
        converted_value: Any = None
        if type(target_type) is list:
            assert type(value) is list
            target_type = target_type[0]  # List element target type is first element of list
            converted_value = parse_param_values(param, target_type, value)
        else:
            converted_value = parse_param_value(param, target_type, value)
        # Apply value to request object field
        rsetattr(req, param.field, converted_value)

    # Serialise request object. Assert at least one parameter given.
    assert req
    serializer = zserio.BitStreamWriter()
    req.write(serializer)
    return serializer.getByteArray()
