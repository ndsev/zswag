"""Exercise the installed binary and Python wrapper without a network service."""

import pickle
from pathlib import Path
from tempfile import TemporaryDirectory

import zserio
from zswag import HTTPConfig, OAClient
from zswag.pyzswagcl import parse_openapi_config

assert issubclass(OAClient, zserio.ServiceInterface)
config = HTTPConfig()
assert config.header("X-Test", "yes") is config
assert isinstance(pickle.loads(pickle.dumps(config)), HTTPConfig)
with TemporaryDirectory() as tmp:
    path = Path(tmp) / "api.yaml"
    path.write_text("openapi: 3.0.0\nservers: [{url: 'http://localhost/'}]\npaths: {}\n")
    assert parse_openapi_config(str(path)).servers == ["http://localhost/"]
    assert OAClient(str(path), is_local_file=True, config=config).config().servers == ["http://localhost/"]
