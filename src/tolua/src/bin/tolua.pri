tolua.name = tolua generator
tolua.input = TOLUA_PKG
tolua.output = ${QMAKE_FILE_IN_BASE}.tolua.cpp
tolua.depends = $$TOLUA_DEPS $$top_builddir/tolua.exe
tolua.commands = $$top_builddir/tolua -n $${TOLUA_PKGNAME} -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
tolua.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += tolua
