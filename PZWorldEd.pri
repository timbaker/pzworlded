# See the README file for instructions about setting the install prefix.
isEmpty(PREFIX):PREFIX = /usr/local
isEmpty(LIBDIR):LIBDIR = $${PREFIX}/lib

macx {
    # Do a universal build when possible
    contains(QT_CONFIG, ppc):CONFIG += x86 ppc
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

DEFINES += ZOMBOID WORLDED
