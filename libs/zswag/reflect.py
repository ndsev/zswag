import inspect
import base64
import zserio
import struct
import functools
from enum import Enum
from typing import Type, Tuple, Any, Dict, Union, Optional, List, get_type_hints, Iterator
from pyzswagcl import OAMethod, OAParam, OAParamFormat, ZSERIO_REQUEST_PART_WHOLE
from re import compile as re
from zserio.typeinfo import TypeInfo, MemberInfo, TypeAttribute, MemberAttribute

NONE_T = type(None)
SCALAR_T = (bool, int, float, str)
TYPE_INFO_CACHE = {}


class NotInstantiableReason(RuntimeError):
    def __init__(self, member_name: str, member_type: str, method_name: str = ""):
        super(NotInstantiableReason, self).__init__("\n" + inspect.cleandoc(f"""
        WARNING: Generating method `{method_name or 'unknown'}`:
        ----------------------------{"-"*len(method_name or 'unknown')}--
        
            The request must be passed as a blob, since the
            member `{member_name}` has non-flattable type
            `{member_type}`.
            
            Please add the following argument to zswag.gen,
            and remove any default parameter specifiers which
            might affect the configuration of `method_name`:
                
                -c {method_name}:blob
        """))
        self.member_name = member_name
        self.member_type = member_type


# Table here: https://docs.python.org/3/library/struct.html#format-characters
C_STRUCT_LITERAL_PER_TYPE_AND_SIZE: Dict[Tuple[Any, int], str] = {
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


# Recursive getattr function, adopted from SO:
#  https://stackoverflow.com/questions/31174295/getattr-and-setattr-on-nested-subobjects-chained-properties
def rgetattr(obj, attr):
    def _getattr(obj, attr):
        if not hasattr(obj, attr):
            raise RuntimeError(
                f"\nERROR: `{attr}` does not exist not in {obj.__name__}; choices are ..." +
                "".join("\n  * "+choice for choice in dir(obj) if not choice.startswith("__")))
        return getattr(obj, attr)
    return functools.reduce(_getattr, [obj] + attr.split('.'))


# Get the request type for a zserio service method.
def service_method_request_type(service_instance: Any, method_name: str) -> Any:
    zserio_impl_function = getattr(service_instance, f"_{to_snake(method_name)}_impl")
    result = get_type_hints(zserio_impl_function)["request"]
    assert inspect.isclass(result)
    return result


# Adopted from zserio PythonSymbolConverter
def to_snake(s: str, patterns=(re("([a-z])([A-Z])"), re("([0-9A-Z])([A-Z][a-z])"))):
    for p in patterns:
        s = p.sub(r"\1_\2", s)
    return s.lower()


# Returns true if t is atomic (not a compound struct),
# false otherwise.
def is_scalar(t: TypeInfo):
    return t.py_type in SCALAR_T or issubclass(t.py_type, Enum)


# Retrieve zserio type info for a schema class -
# result is cached for maximum performance.
def cached_type_info(zserio_t) -> Optional[TypeInfo]:
    if not hasattr(zserio_t, "type_info"):
        return None
    key = f"{zserio_t.__module__}.{zserio_t.__qualname__}"
    if key not in TYPE_INFO_CACHE:
        TYPE_INFO_CACHE[key] = zserio_t.type_info()
    return TYPE_INFO_CACHE[key]


# Recursively find a member info (e.g. "myField1.myField2") in
# a TypeInfo. Member names may be pythonic. The input field path
# must be with original (not necessarily pythonic) schema field
# names. The first output tuple entry is the pythonic conversion
# ("my_field_1.my_field_2").
def find_field(t: TypeInfo, field: str, offset: int = 0, py_path="") -> Tuple[str, Optional[MemberInfo]]:
    next_dot = field.find(".", offset)
    subfield = field[offset:next_dot] if next_dot >= 0 else field[offset:]
    for _, member_info in members(t):
        if member_info.schema_name == subfield:
            if offset > 0:
                py_path += "."
            py_path += member_info.attributes[MemberAttribute.PROPERTY_NAME]
            if next_dot < 0:
                return py_path, member_info
            return find_field(member_info.type_info, field, next_dot+1, py_path)
    return "", None


# Simply yields all MemberInfo for a TypeInfo.
def members(t: TypeInfo, recursive=False, field_prefix="") -> Iterator[MemberInfo]:
    if TypeAttribute.FIELDS in t.attributes:
        for member in t.attributes[TypeAttribute.FIELDS]:
            field_name = field_prefix+"." if field_prefix else ""
            field_name += member.schema_name
            yield field_name, member
            if recursive:
                yield from members(member.type_info, recursive, field_name)


# Checks whether is a type is not trivially constructable:
# It (or a subtype) might take extra parameters, or be
# an array of non-scalar types.
def check_uninstantiable(t: TypeInfo, field_name="", recursive=False) -> Optional[NotInstantiableReason]:
    if TypeAttribute.PARAMETERS in t.attributes and t.attributes[TypeAttribute.PARAMETERS]:
        return NotInstantiableReason(field_name, t.schema_name)
    member_info: MemberInfo
    for _, member_info in members(t):
        subfield_name = f"{field_name}.{member_info.schema_name}" if field_name else member_info.schema_name
        if MemberAttribute.ARRAY_LENGTH in member_info.attributes and not is_scalar(member_info.type_info):
            return NotInstantiableReason(subfield_name, t.schema_name+"[]")
        if recursive:
            if reason := check_uninstantiable(member_info.type_info, subfield_name, recursive):
                return reason


# Returns a fully recursively initialized class instance
# of a zserio schema type, if possible.
def instantiate(t: Type) -> Any:
    result_instance = t()
    type_info = cached_type_info(t)
    if reason := check_uninstantiable(type_info):
        raise reason
    for _, member_info in members(type_info):
        field_name: str = member_info.attributes[MemberAttribute.PROPERTY_NAME]
        member_type_info = member_info.type_info
        if not is_scalar(member_type_info):
            compound_field_value = instantiate(member_type_info.py_type)
            setattr(result_instance, f"_{field_name}_", compound_field_value)
    return result_instance


# Get a byte buffer from a string which is encoded in a given format
def str_to_bytes(s: str, fmt: OAParamFormat) -> bytes:
    if fmt == OAParamFormat.BASE64:
        return base64.b64decode(s)
    elif fmt == OAParamFormat.BASE64URL:
        return base64.urlsafe_b64decode(s)
    elif fmt == OAParamFormat.HEX:
        return bytes.fromhex(s)
    else:  # if fmt in (OAParamFormat.BINARY, OAParamFormat.STRING):
        return bytes(s, encoding="raw_unicode_escape")


# Convert a single passed parameter value to it's correct type
def parse_param_value(param: OAParam, target_type: Type, value: str) -> Any:
    # Check if it's an enum ...
    if issubclass(target_type, Enum):
        return target_type(parse_param_value(param, int, value))
    # Check if the parameter format is native string conversion
    if param.format == OAParamFormat.STRING:
        if target_type is bool:
            return bool(int(value))
        return target_type(value)
    # Is it hex-to-int conversion?
    if param.format == OAParamFormat.HEX and target_type is int:
        return int(value, 16)
    # It is some kind of blob conversion!
    value_as_bytes = str_to_bytes(value, param.format)
    # Is it a serialized C type (char/uchar/float/double/long...)?
    struct_literal_key = (target_type, len(value_as_bytes))
    if struct_literal_key in C_STRUCT_LITERAL_PER_TYPE_AND_SIZE:
        return target_type(struct.unpack(
            C_STRUCT_LITERAL_PER_TYPE_AND_SIZE[struct_literal_key],
            value_as_bytes)[0])
    # Is it a string blob?
    if target_type is str:
        return str(value_as_bytes, 'utf-8')
    raise RuntimeError(f"Cannot convert {len(value_as_bytes)} to {target_type}")


# Convert an array of passed parameter values to their correct type
def parse_param_values(param: OAParam, target_type: Type, value: List[str]) -> List[Any]:
    return [parse_param_value(param, target_type, item) for item in value]


# Get a blob for a zserio request type, a set of request parameter values
# and an OpenAPI method path spec.
def request_object_blob(*, req_t: Type, headers: Dict[str, Any], spec: OAMethod, **kwargs) -> bytes:
    # Lazy instantiation of request object and type info
    req: Optional[req_t] = None
    req_t_info = cached_type_info(req_t)
    # Apply parameters
    param_name: str
    param: OAParam
    for param_name, param in spec.parameters.items():
        # Get raw string value
        value: Union[str, List[str]] = param.default_value
        param_name = param_name.replace("-", "_")
        if param_name in kwargs:
            value = kwargs[param_name]
        else:
            if param_name in headers:
                value = headers[param_name]
        # Convert string value to whole blob
        if param.field == ZSERIO_REQUEST_PART_WHOLE:
            return str_to_bytes(value, param.format)
        # First non-whole request parameter: Synthetic request object
        if not req:
            req = instantiate(req_t)
        # Convert string value to correct type
        pythonic_field_path, target_type = find_field(req_t_info, param.field)
        if not target_type:
            raise RuntimeError(f"Could not find field {param.field}!")
        if MemberAttribute.ARRAY_LENGTH in target_type.attributes:
            converted_value = parse_param_values(param, target_type.type_info.py_type, value)
        else:
            converted_value = parse_param_value(param, target_type.type_info.py_type, value)
        # Apply value to request object field
        rsetattr(req, pythonic_field_path, converted_value)
    # Serialise request object. Assert at least one parameter given.
    assert req
    serializer = zserio.BitStreamWriter()
    req.write(serializer)
    return serializer.byte_array
