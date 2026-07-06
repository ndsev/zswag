# OpenAPI Generator (`zswag.gen`)

After installing `zswag` (see [`python.md`](python.md)), the command `python -m zswag.gen` generates an OpenAPI YAML from a zserio service definition. The generated YAML is what `OAServer` serves and what all zswag clients consume.

## Synopsis

```
usage: Zserio OpenApi YAML Generator [-h] -s service-identifier -i zserio-or-python-path
                                     [-r zserio-src-root-dir]
                                     [-p top-level-package]
                                     [-c tags [tags ...]]
                                     [-o output]
                                     [-b BASE_CONFIG_YAML]
```

## Options

### `-s` / `--service` (required)

Fully qualified zserio service identifier.

```
-s my.package.ServiceClass
```

### `-i` / `--input` (required)

Either:

- **(A)** Path to a zserio `.zs` file. Must be either a top-level entrypoint (e.g. `all.zs`) or a subpackage (e.g. `services/myservice.zs`) used together with `--zserio-source-root|-r <dir>`.
- **(B)** Path to the parent dir of a zserio Python package.

```
-i path/to/schema/main.zs                 # (A)
-i path/to/python/package/parent          # (B)
```

### `-r` / `--zserio-source-root`

When `-i` specifies a `.zs` file (Option A), indicates the directory passed to zserio's `-src` flag. Defaults to the parent dir of the given file.

### `-p` / `--package`

When `-i` specifies a `.zs` file (Option A), indicates a specific top-level zserio package name.

```
-p zserio_pkg_name
```

### `-c` / `--config`

Configuration tags. Syntax:

```
[(service-method-name):](comma-separated-tags)
```

Supported tags:

| Tag | Effect |
|---|---|
| `get` `put` `post` `delete` | HTTP method (default: `post`). |
| `query` `path` `header` `body` | Parameter location (default: `query` for `get`, `body` otherwise). |
| `flat` `blob` | Flatten the request object into its scalar fields, OR pass it whole as a blob. |
| `(param-specifier)` | Specify name/format/location for a specific request part — see below. |
| `security=(name)` | Use a specific security scheme. The scheme details must be provided via `--base-config-yaml`. |
| `path=(method-path)` | Override the method path. May contain placeholders for path params. |

A **param-specifier** has the schema:

```
(field?name=...
       &in=[path|body|query|header]
       &format=[binary|base64|hex]
       [&style=...]
       [&explode=...])
```

Examples:

```bash
# Expose all methods as POST, but getLayerByTileId as GET with flat path-parameters:
-c post getLayerByTileId:get,flat,path

# For myMethod, put the whole request blob into a query "data" parameter as base64:
-c myMethod:*?name=data&in=query&format=base64

# For myMethod, set the "AwesomeAuth" auth scheme:
-c myMethod:security=AwesomeAuth

# For myMethod, provide a path with a placeholder for myField:
-c 'myMethod:path=/my-method/{param}, myField?name=param&in=path&format=string'
```

Notes:

- HTTP method defaults to `post`.
- `in` defaults to `query` for `get`, `body` otherwise.
- If a method uses a parameter specifier, the `flat`, `body`, `query`, `path`, `header`, and body tags are ignored.
- `flat` is meaningful only with `query` or `path`.
- An unspecific tag list (no method name) affects defaults only for following, not preceding, specialised assignments.

### `-o` / `--output`

Output file path. Defaults to stdout.

### `-b` / `--base-config-yaml`

Base YAML for fully or partially substituting `--config`, plus extra OpenAPI metadata. Schema:

```yaml
method:                          # optional method tags dictionary
  <method-name|*>: <list of config tags>
securitySchemes: ...             # optional OpenAPI securitySchemes
info: ...                        # optional OpenAPI info section
servers: ...                     # optional OpenAPI servers section
security: ...                    # optional OpenAPI global security
```

## End-to-end example

Given:

```
package services;

struct Request { int32 value; };
struct Response { int32 value; };

service MyService {
  Response myApi(Request);
};
```

Generate `api.yaml`:

```bash
cd myapp
python -m zswag.gen -s services.MyService -i services.zs -o api.yaml
```

Customise via `-c`:

```bash
# All methods as GET, flat path-parameters:
python -m zswag.gen -s services.MyService -i services.zs -c get,flat,path -o api.yaml
```

To override only one method:

```bash
python -m zswag.gen -s services.MyService -i services.zs \
  -c post getLayerByTileId:get,flat,path \
  -o api.yaml
```

## Documentation extraction

When invoked with `-i <zserio-file>`, `zswag.gen` populates the OpenAPI service / method / request / response descriptions from doc-strings extracted from the zserio sources.

For structs and services, the documentation is expected to be enclosed by `/*! .... !*/` markers preceding the declaration:

```c
/*!
### My Markdown Struct Doc
I choose to __highlight__ this word.
!*/

struct MyStruct {
    ...
};
```

For service methods, a single-line doc-string immediately precedes the declaration:

```c
/** This method is documented. */
ReturnType myMethod(ArgumentType);
```
