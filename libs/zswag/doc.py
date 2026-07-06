"""Extract zserio source comments for generated OpenAPI descriptions.

``zswag.gen`` uses this module to enrich generated OpenAPI YAML with comments
from ``.zs`` sources. The extraction deliberately stays lightweight: it scans
the zserio package tree as text and applies a small set of regular expressions
for services, structs, and RPC methods.
"""

from enum import Enum
from typing import List, Dict
import re
import glob
import os

RPC_DOC_PATTERN = r"""
        service\s+{service_name}\s+\{{   # service MyService {{ (double-braces due to later .format())
            (?:\n|.)*                    #   ...
            /\*\*?\s*                    #   /**
                ((?:[^*]|\*[^/])*)       #     (doc-string) -> captured
            \*/                          #   */
            \s*([A-Za-z0-9_]*)\s+        #   (return-type) -> captured
            {rpc_name}                   #   method-name
                \s*\(\s*                 #   (
                    ([A-Za-z0-9_]*)      #     (argument-type) -> captured
                \s*\)                    #   )
"""

STRUCT_PATTERN = r"""
          /\*\!                          # /*!
            ((?:[^!]|![^*]|!\*[^/])*)    #   (doc-string) -> captured
          !\*/\s+                        # !*/
          struct\s+{name}                # struct NAME
"""

SERVICE_PATTERN = r"""
          /\*\!                          # /*!
            ((?:[^!]|![^*]|!\*[^/])*)    #   (doc-string) -> captured
          !\*/\s+                        # !*/
          service\s+{name}               # service NAME
"""


class IdentType(Enum):
    """Kind of zserio identifier looked up by :func:`get_doc_str`.

    ``STRUCT`` and ``SERVICE`` expect a fully qualified type/service name.
    ``RPC`` expects a fully qualified ``service.method`` name so the extractor
    can match the method inside the correct service declaration.
    """

    STRUCT = 0
    SERVICE = 1
    RPC = 2


# Cache package roots to their concatenated zserio sources so repeated
# documentation lookups during one OpenAPI generation pass stay cheap.
zs_pkg_cache: Dict[str, str] = {}


def get_amalgamated_zs(pkg_path):
    """Return all ``.zs`` sources below *pkg_path* concatenated into one string.

    The result is cached by package path. This keeps repeated service, method,
    and struct doc lookups from walking the same package tree over and over
    during OpenAPI generation.
    """

    global zs_pkg_cache
    if pkg_path in zs_pkg_cache:
        return zs_pkg_cache[pkg_path]
    zs_files = glob.glob(os.path.join(pkg_path, "**/*.zs"), recursive="True")
    result = ""
    for zs_file_path in zs_files:
        with open(zs_file_path) as zs_file:
            result += zs_file.read() + "\n"
    zs_pkg_cache[pkg_path] = result
    return result


def get_doc_str(*, ident_type: IdentType, pkg_path: str, ident: str, fallback: List[str] = None) -> List[str]:
    """Return captured zserio documentation groups for an identifier.

    The function searches all ``.zs`` files below ``pkg_path`` using a pattern
    selected by ``ident_type``:

    * ``STRUCT`` looks up ``path.to.package.StructName``.
    * ``SERVICE`` looks up ``path.to.package.ServiceName``.
    * ``RPC`` looks up ``path.to.package.ServiceName.methodName`` and captures
      the method documentation, return type, and request argument type.

    ``fallback`` is returned unchanged when ``pkg_path`` is empty, the
    identifier is malformed, the identifier type is unsupported, or no match is
    found. Callers use this to keep OpenAPI generation deterministic even when
    source comments are unavailable.
    """

    if fallback is None:
        fallback = []
    if not pkg_path:
        return fallback
    zs_src = get_amalgamated_zs(pkg_path)
    ident_parts = ident.split(".")
    pattern_format_replacements = {}
    if ident_type == IdentType.STRUCT:
        if not ident_parts:
            print("[ERROR] Need at least one identifier part to find struct docs.")
            return fallback
        pattern = STRUCT_PATTERN
        pattern_format_replacements["name"] = ident_parts[-1]
    elif ident_type == IdentType.SERVICE:
        if not ident_parts:
            print("[ERROR] Need at least one identifier part to find service docs.")
            return fallback
        pattern = SERVICE_PATTERN
        pattern_format_replacements["name"] = ident_parts[-1]
    elif ident_type == IdentType.RPC:
        if not ident_parts or len(ident_parts) < 2:
            print("[ERROR] Need at least tow identifiers (service.rpc-name) to find RPC docs.")
            return fallback
        pattern = RPC_DOC_PATTERN
        pattern_format_replacements["service_name"] = ident_parts[-2]
        pattern_format_replacements["rpc_name"] = ident_parts[-1]
    else:
        print("[ERROR] get_doc_str: Unsupported identifier type!")
        return fallback
    compiled_pattern = re.compile(pattern.format(**pattern_format_replacements), re.X)
    match = compiled_pattern.search(zs_src)
    if match:
        return list(match.groups())
    else:
        return fallback


def md_filter_definition(md: str) -> str:
    """Remove leading Markdown ``Definition`` headings from extracted text."""

    return re.sub(r"\n*\*\*[Dd]efinitions?[:\s]*\*\*\n*", "", md.strip()).strip()
