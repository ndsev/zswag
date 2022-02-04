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
    """
    Use these enum entries with `get_doc_str()`.
    """
    STRUCT = 0
    SERVICE = 1
    RPC = 2


"""
Caches glob.glob() results for *.zs file searches in get_doc_str().
The dictionary points from a package path to amalgamated zserio code
for that package.
"""
zs_pkg_cache: Dict[str, str] = {}


def get_amalgamated_zs(pkg_path):
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
    f"""
    Get a docstring for a particular zserio identifier. This method searches all .zs-files
    under `pkg_path` for a specific pattern given by `ident_type` and `ident`

    The following patterns are looked for:

      With `ident_type` IdentType.STRUCT:
        With ident as "path.to.package.NAME":
         {STRUCT_PATTERN}

      With `ident_type` IdentType.SERVICE)
        Same as STRUCT, except looking for "service NAME".

      With `ident_type` IdentType.RPC)
        With ident as "path.to.service.SERVICE.NAME":
         {RPC_DOC_PATTERN}

    The list of all capture group values is returned.
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
    return re.sub(r"\n*\*\*[Dd]efinitions?[:\s]*\*\*\n*", "", md.strip()).strip()
