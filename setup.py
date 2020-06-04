import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

required_url = []
required = []
with open("requirements.txt", "r") as freq:
    for line in freq.read().split():
        if "://" in line:
            required_url.append(line)
        else:
            required.append(line)

packages = setuptools.find_packages("src")

setuptools.setup(
    name="zswag",
    version="0.5.0",
    url="https://github.com/klebert-engineering/zswag",
    author="Klebert Engineering",
    author_email="j.birkner@klebert-engineering.de",

    description="Convenience functionality to create python modules from zserio services at warp speed.",
    long_description=long_description,
    long_description_content_type="text/markdown",

    package_dir={'': 'src'},
    packages=packages,

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
