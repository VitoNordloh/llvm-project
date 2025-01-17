// RUN: rm -rf %t
// RUN: mkdir -p %t/Incomplete.framework/Headers
// RUN: echo '// IncompleteReal.h' > %t/Incomplete.framework/Headers/IncompleteReal.h
// RUN: sed -e "s@INPUT_DIR@%/S/Inputs@g" -e "s@OUT_DIR@%/t@g" %S/Inputs/vfsoverlay.yaml > %t.yaml
// RUN: not %clang_cc1 -Werror -fmodules -fimplicit-module-maps -fmodules-cache-path=%t \
// RUN:     -ivfsoverlay %t.yaml -F %t -fsyntax-only %s 2>&1 | FileCheck %s

// FIXME: PR43272
// XFAIL: windows

@import Incomplete;
// CHECK: umbrella header for module 'Incomplete' {{.*}}IncompleteVFS.h
// CHECK: umbrella header for module 'Incomplete' {{.*}}IncompleteReal.h
// CHECK: could not build module 'Incomplete'
