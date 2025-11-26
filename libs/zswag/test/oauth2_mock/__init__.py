"""
OAuth2 Mock Server for testing OAuth 1.0 signature and HTTP Basic Auth.
"""

import zserio
import sys
from os.path import dirname, abspath

# Export working directory for tests
working_dir = dirname(abspath(__file__))

# Add working directory to sys.path so oauth_test can be imported
if working_dir not in sys.path:
    sys.path.insert(0, working_dir)

# Generate oauth_test API when module is imported
zserio.generate(
    zs_dir=working_dir,
    main_zs_file="oauth_test.zs",
    gen_dir=working_dir,
    extra_args=["-withTypeInfoCode"])
