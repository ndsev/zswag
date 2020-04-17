import os
import subprocess
import requests
import sys
from zipfile import ZipFile


zs_jar_path = None


def setup(ver="2.0.0-pre1"):
    """
    Install zserio.jar and `zserio` runtime module in current PYTHONPATH.
    If the version is not cached, it will be downloaded from github.
    """
    global zs_jar_path
    download_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "download")
    zs_ver_path = os.path.join(download_path, ver)
    desired_jar_path = os.path.join(zs_ver_path, "zserio.jar")
    if zs_jar_path == desired_jar_path:
        return
    zs_jar_path = os.path.join(zs_ver_path, "zserio.jar")
    zs_runtime_path = os.path.join(zs_ver_path, "runtime_libs", "python")
    if not os.path.isdir(zs_ver_path):
        os.mkdir(zs_ver_path)
    if not os.path.isfile(zs_jar_path):
        print(f"Downloading zserio jar/runtime at version {ver}.")
        downloads = ((
            os.path.join(zs_ver_path, "bin.zip"),
            f"https://github.com/ndsev/zserio/releases/download/v{ver}/zserio-{ver}-bin.zip"
        ), (
            os.path.join(zs_ver_path, "runtime.zip"),
            f"https://github.com/ndsev/zserio/releases/download/v{ver}/zserio-{ver}-runtime-libs.zip"
        ))
        for zip_path, zip_url in downloads:
            with open(zip_path, "wb") as zipfile:
                zip_response = requests.get(zip_url)
                zipfile.write(zip_response.content)
            with ZipFile(zip_path, "r") as zipfile:
                zipfile.extractall(zs_ver_path)
            os.remove(zip_path)
    print(f"Using zserio v{ver} (jar: {zs_jar_path}, runtime: {zs_runtime_path}).")
    correct_path_already_added = False
    for path in sys.path[:]:
        if path.startswith(download_path):
            if path == zs_runtime_path:
                correct_path_already_added = True
                continue
            sys.path.remove(path)
    if not correct_path_already_added:
        sys.path.append(zs_runtime_path)


def package(src_file: str = "", *, package_prefix: str = ""):
    """
    Description

        Generate python sources from zserio code, and add the
        generated package to pythonpath. The generated sources
        will be placed under .../src/.zs-python-package/.
        This path will be added to sys.path.

        **Note:** Make sure to call `setup()` before
         calling `package()`.

    Example:

        ```
        import zswag
        zswag.setup()
        zswag.package("myfile.zs")

        from myfile import *
        ```

    :param src_file: Source zserio file.
    :return: True if succesfull, False otherwise.
    """
    global zs_jar_path
    if not zs_jar_path:
        print("""
        ERROR: Zserio not installed. Call `setup()` before
        running `package()`, or set `zswag.zs_jar_path`.
        """)
        return False
    zs_pkg_path = os.path.dirname(os.path.abspath(src_file))
    zs_build_path = os.path.join(zs_pkg_path, ".zs-python-package")
    subprocess.run([
        "java", "-jar", zs_jar_path,
        "-src", zs_pkg_path,
        "-python", zs_build_path,
        *(("-setTopLevelPackage", package_prefix) if package_prefix else tuple()),
        os.path.basename(src_file)])
    if zs_build_path not in sys.path:
        sys.path.append(zs_build_path)
    return True
