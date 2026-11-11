#!/bin/sh
exec /usr/bin/python3 -c 'import setuptools,runpy,sys; sys.argv[0]="/tmp/telegram-sysroot/usr/bin/g-ir-scanner"; runpy.run_path(sys.argv[0],run_name="__main__")' "$@"
