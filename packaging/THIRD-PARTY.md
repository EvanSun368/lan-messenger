# Third-party runtime notices

This development build bundles CPython, PySide6/Shiboken6 and Qt libraries, and a PyInstaller bootloader. It is not a standalone proprietary runtime and is not affiliated with Feige.

- Python: PSF license. Source and license: https://www.python.org/downloads/source/
- PySide6/Shiboken6: LGPLv3/GPLv3 or commercial terms. Source: https://code.qt.io/pyside/pyside-setup.git/
- Qt: applicable LGPL/GPL and third-party terms. Source and corresponding releases: https://download.qt.io/archive/qt/ ; licensing: https://www.qt.io/licensing/open-source-lgpl-obligations
- PyInstaller: GPL with bootloader exception allowing bundled applications. https://pyinstaller.org/en/stable/license.html

GPL-3, LGPL-3 and LGPL-2.1 texts accompany this preview. These notices do not replace each dependency's applicable terms. Keep all notices when sharing. The owner's own application license has not been selected; do not assume permission for public redistribution.

The bundle uses dynamic Qt DLLs in _internal and does not prohibit replacing LGPL components or reverse engineering for debugging such modifications. To rebuild the application with replacement dependencies, use the source in https://github.com/EvanSun368/lan-messenger/tree/build/windows-portable-0.4.0 . Review exact dependency licenses and corresponding source obligations before any public distribution.

This is an unsigned development preview. No security software should be disabled to run it.
