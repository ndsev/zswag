"""Reflection helpers shared by zswag's Python server and generator.

The functions in this module translate between OpenAPI parameter values and
zserio Python request objects. They rely on zserio's generated type information
(``type_info()``) to inspect fields, instantiate request objects, and serialize
the final request blob consumed by generated zserio service code.
"""

import inspect
import base64
import zserio
import struct
import functools
from enum import Enum
from typing import Type, Tuple, Any, Dict, Union, Optional, List, get_type_hints, Iterator
from .pyzswagcl import OAMethod, OAParam, OAParamFormat, ZSERIO_REQUEST_PART_WHOLE
from re import compile as re
from zserio.typeinfo import TypeInfo, MemberInfo, TypeAttribute, MemberAttribute

NONE_T = type(None)
SCALAR_T = (bool, int, float, str)
TYPE_INFO_CACHE = {}


class NotInstantiableReason(RuntimeError):
    """Explains why a zserio type cannot be synthesized from flat parameters.

    Some request objects cannot be created generically because they require
    constructor parameters or contain arrays of compound objects. The OpenAPI
    generator uses this exception to tell users to expose the request as a blob
    instead of flattening it into scalar parameters.
    """

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
    """Set a possibly dotted attribute path on an object.

    ``attr`` may be a simple attribute name or a dotted path such as
    ``"outer.inner.value"``. Intermediate objects are resolved with
    :func:`rgetattr`; only the final segment is assigned.
    """
    pre, _, post = attr.rpartition('.')
    return setattr(rgetattr(obj, pre) if pre else obj, post, val)


# Recursive getattr function, adopted from SO:
#  https://stackoverflow.com/questions/31174295/getattr-and-setattr-on-nested-subobjects-chained-properties
def rgetattr(obj, attr):
    """Resolve a possibly dotted attribute path on an object.

    Each intermediate object must expose the next path segment. This helper
    does not create missing objects or resolve schema names to Python names.
    """
    def _getattr(obj, attr):
        if not hasattr(obj, attr):
            raise RuntimeError(
                f"\nERROR: `{attr}` does not exist not in {obj.__name__}; choices are ..." +
                "".join("\n  * "+choice for choice in dir(obj) if not choice.startswith("__")))
        return getattr(obj, attr)

    return functools.reduce(_getattr, [obj] + attr.split('.'))


def service_method_request_type(service_instance: Any, method_name: str) -> Any:
    """Return the generated request class for a zserio service method.

    zserio's generated Python service implementation methods are type annotated.
    This helper looks up the private ``_<method>_impl`` function, reads its
    ``request`` annotation, and returns the request class used by the OpenAPI
    generator/server path.
    """
    zserio_impl_function = getattr(service_instance, f"_{to_snake(method_name)}_impl")
    result = get_type_hints(zserio_impl_function)["request"]
    assert inspect.isclass(result)
    return result


# Adopted from zserio PythonSymbolConverter
def to_snake(s: str, patterns=(re("([a-z])([A-Z])"), re("([0-9A-Z])([A-Z][a-z])"))):
    """Convert a zserio-style camelCase/PascalCase identifier to snake_case."""
    for p in patterns:
        s = p.sub(r"\1_\2", s)
    return s.lower()


def is_scalar(t: TypeInfo):
    """Return whether zserio type info represents a scalar or enum value."""
    return t.py_type in SCALAR_T or issubclass(t.py_type, Enum)


def cached_type_info(zserio_t) -> Optional[TypeInfo]:
    """Return cached zserio ``TypeInfo`` for a generated schema class.

    The zserio Python runtime computes type information from generated metadata.
    Caching avoids repeated reflection work when generating large OpenAPI files
    or handling many incoming requests. Types without ``type_info`` return
    ``None``. Entries are keyed by module and qualified class name and retained
    for the process lifetime; regenerating a class under the same name does not
    invalidate its entry.
    """
    if not hasattr(zserio_t, "type_info"):
        return None
    key = f"{zserio_t.__module__}.{zserio_t.__qualname__}"
    if key not in TYPE_INFO_CACHE:
        TYPE_INFO_CACHE[key] = zserio_t.type_info()
    return TYPE_INFO_CACHE[key]


def find_field(t: TypeInfo, field: str, offset: int = 0, py_path="") -> Tuple[str, Optional[MemberInfo]]:
    """Find a dotted zserio schema field in ``TypeInfo``.

    ``field`` uses schema names, not Python property names. The returned tuple
    contains the equivalent dotted Python attribute path and the matching
    ``MemberInfo``. If no field matches, ``("", None)`` is returned.
    For example, ``myField1.myField2`` becomes ``my_field_1.my_field_2``.
    """
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


def members(t: TypeInfo, recursive=False, field_prefix="") -> Iterator[MemberInfo]:
    """Yield ``(schema_field_name, MemberInfo)`` pairs from zserio type info.

    When ``recursive`` is true, nested compound members are traversed and field
    names are emitted as dotted schema paths. Each compound member is yielded
    before its descendants; this is not a leaf-only traversal. Recursive schema
    cycles are not detected.
    """
    if TypeAttribute.FIELDS in t.attributes:
        for member in t.attributes[TypeAttribute.FIELDS]:
            field_name = field_prefix+"." if field_prefix else ""
            field_name += member.schema_name
            yield field_name, member
            if recursive:
                yield from members(member.type_info, recursive, field_name)


def check_uninstantiable(t: TypeInfo, field_name="", recursive=False) -> Optional[NotInstantiableReason]:
    """Check whether a type can be auto-instantiated from flat parameters.

    Return the first unsupported constructor/array shape, or ``None``.

    Auto-instantiation is only safe for request objects with parameterless
    constructors and scalar arrays. Compound arrays and parameterized zserio
    structs require user data the generic OpenAPI path cannot synthesize.
    By default only this type and its direct array fields are checked; set
    ``recursive=True`` to inspect nested compound fields too. This check does
    not execute constructors or prove arbitrary schema constraints are valid.
    """
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


def instantiate(t: Type) -> Any:
    """Create a recursively initialized instance of a generated zserio class.

    Non-scalar fields are recursively instantiated so flattened HTTP parameters
    can later be assigned into nested request objects. Raises
    :class:`NotInstantiableReason` for unsupported array/parameter shapes
    reached after construction. The generated no-argument constructor runs
    first, so its own exceptions can propagate before that check.
    """
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


def str_to_bytes(s: str, fmt: OAParamFormat) -> bytes:
    """Decode base64/base64url/hex, or encode other formats as raw Unicode bytes.

    Decoder errors propagate. This helper does not perform zserio validation.
    """
    if fmt == OAParamFormat.BASE64:
        return base64.b64decode(s)
    elif fmt == OAParamFormat.BASE64URL:
        return base64.urlsafe_b64decode(s)
    elif fmt == OAParamFormat.HEX:
        return bytes.fromhex(s)
    else:  # if fmt in (OAParamFormat.BINARY, OAParamFormat.STRING):
        return bytes(s, encoding="raw_unicode_escape")


def parse_param_value(param: OAParam, target_type: Type, value: str) -> Any:
    """Convert one raw OpenAPI parameter value into a target Python type.

    String-format booleans use ``bool(int(value))``; numeric text other than
    zero is true. Enums convert through their integer value. Hex integers use
    ``int(value, 16)``. Other binary numeric encodings are signed, big-endian
    C-width values, not arbitrary zserio bitstreams; strings decode as UTF-8.
    Unsupported type/byte-length combinations raise ``RuntimeError`` and
    conversion or decoder exceptions propagate.
    """
    # Enums share the numeric conversion path before resolving the member.
    if issubclass(target_type, Enum):
        return target_type(parse_param_value(param, int, value))

    # String and hex integers bypass binary decoding.
    if param.format == OAParamFormat.STRING:
        if target_type is bool:
            return bool(int(value))
        return target_type(value)

    if param.format == OAParamFormat.HEX and target_type is int:
        return int(value, 16)

    # Binary numeric encodings select a fixed-width, network-order struct.
    value_as_bytes = str_to_bytes(value, param.format)
    struct_literal_key = (target_type, len(value_as_bytes))
    if struct_literal_key in C_STRUCT_LITERAL_PER_TYPE_AND_SIZE:
        return target_type(struct.unpack(
            C_STRUCT_LITERAL_PER_TYPE_AND_SIZE[struct_literal_key],
            value_as_bytes)[0])

    if target_type is str:
        return str(value_as_bytes, 'utf-8')

    raise RuntimeError(f"Cannot convert {len(value_as_bytes)} to {target_type}")


def parse_param_values(param: OAParam, target_type: Type, value: List[str]) -> List[Any]:
    """Convert a repeated OpenAPI parameter into a list of typed values."""
    return [parse_param_value(param, target_type, item) for item in value]


def request_object_blob(*, req_t: Type, headers: Dict[str, Any], spec: OAMethod, **kwargs) -> bytes:
    """Build a serialized zserio request blob from HTTP request data.

    ``spec`` describes where each OpenAPI parameter maps into the zserio request
    object. Whole-request blob parameters are returned directly after decoding.
    For field-level parameters, the function lazily instantiates ``req_t``,
    converts raw parameter/header values, assigns them into the generated object,
    and serializes the object with ``zserio.BitStreamWriter``. Keyword values
    take precedence over headers, then declared defaults. At least one whole
    blob or field mapping is required; a field-only request with no parameters
    reaches an assertion. Conversion, lookup and serialization errors propagate.
    """
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

        # A whole-request mapping supplies the complete payload immediately.
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
