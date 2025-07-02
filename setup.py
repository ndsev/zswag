import setuptools
import sys
import codecs
import os

# Read first argument as version, pass rest on to setuptools
VERSION = sys.argv[1]
del sys.argv[1]

# Determine the directory containing this setup.py file
setup_dir = os.path.dirname(os.path.abspath(__file__))
readme_path = os.path.join(setup_dir, "README.md")

with codecs.open(readme_path, "r", "utf-8") as fh:
    long_description = fh.read()

required_url = []
required = [f"pyzswagcl=={VERSION}"]
requirements_path = os.path.join(setup_dir, "requirements.txt")
with open(requirements_path, "r") as freq:
    for line in freq.read().split():
        if "://" in line:
            required_url.append(line)
        else:
            required.append(line)

setuptools.setup(
    name="zswag",
    version=VERSION,
    url="https://github.com/klebert-engineering/zswag",
    author="Klebert Engineering",
    author_email="team@klebert-engineering.de",

    description="OpenAPI service transport layer for zserio.",
    long_description=long_description,
    long_description_content_type="text/markdown",

    package_dir={'': 'libs'},
    packages=['zswag', 'zswag.test.calc'],
    include_package_data=True,
    package_data={
        "zswag": [
            "test/calc/api.yaml",
            "test/calc/calculator.zs"
        ]
    },

    install_requires=required,
    dependency_links=required_url,
    python_requires='>=3.10',

    license="BSD-3 Clause",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Operating System :: OS Independent",
        "License :: OSI Approved :: BSD License"
     ],
)
