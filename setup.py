import setuptools
import sys
import codecs

# Read first argument as version, pass rest on to setuptools
VERSION = sys.argv[1]
del sys.argv[1]

with codecs.open("README.md", "r", "utf-8") as fh:
    long_description = fh.read()

required_url = []
required = []
with open("requirements.txt", "r") as freq:
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
    author_email="j.birkner@klebert-engineering.de",

    description="Server middleware for implementing zserio services at warp 10.",
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
    python_requires='>=3.6',

    license="BSD-3 Clause",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Operating System :: OS Independent",
        "License :: OSI Approved :: BSD License"
     ],
)
