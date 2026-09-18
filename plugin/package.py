"""Zips a staged package folder into a .ts3_plugin laid out like TFAR's known-good package: an explicit entry
for every directory and DOS attributes on every entry. TeamSpeak's package installer extracted only empty files
from a zip without them.

Usage: python package.py <staged folder> <output .ts3_plugin>
"""

import os
import sys
import time
import zipfile

ATTRIBUTE_DIRECTORY = 0x10
ATTRIBUTE_ARCHIVE = 0x20
# "Made by" MS-DOS, spec version 6.3, as in TFAR's package
CREATE_SYSTEM_DOS = 0
CREATE_VERSION = 63


def add_entry(archive, name, path):
    modified = os.path.getmtime(path)
    info = zipfile.ZipInfo(name, date_time=time.localtime(modified)[:6])
    info.create_system = CREATE_SYSTEM_DOS
    info.create_version = CREATE_VERSION
    if os.path.isdir(path):
        info.external_attr = ATTRIBUTE_DIRECTORY
        info.compress_type = zipfile.ZIP_STORED
        archive.writestr(info, b"")
        return
    info.external_attr = ATTRIBUTE_ARCHIVE
    info.compress_type = zipfile.ZIP_DEFLATED
    with open(path, "rb") as source:
        archive.writestr(info, source.read())


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    staged, output = sys.argv[1], sys.argv[2]
    with zipfile.ZipFile(output, "w") as archive:
        for root, dirs, files in os.walk(staged):
            dirs.sort()
            files.sort()
            relative = os.path.relpath(root, staged).replace(os.sep, "/")
            prefix = "" if relative == "." else relative + "/"
            if prefix:
                add_entry(archive, prefix, root)
            for name in files:
                add_entry(archive, prefix + name, os.path.join(root, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
