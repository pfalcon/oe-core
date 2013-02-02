# Automatically rewrite shebang (#!) lines in shell scripts
# to point to replace "/bin" with ${base_bindir}.
#
# The usecase is building completely self-contained packages
# with non-standard --prefix, installed on a host system where
# one doesn't have complete control over file system.


inherit package
PACKAGEFUNCS += " do_package_shebang "

def package_check_shebang(path, package, d):
    print path, package
    if package.endswith("-native"):
        return
    if package.endswith("-cross"):
        return
    try:
        f = open(path)
    except:
        return True
    l = f.readline()
    if len(l) > 30:
        return True
    if l[0:2] != "#!":
        return True
    l = l[2:]
    if l.startswith("/bin"):
        data = f.read()
        f.close()
        f = open(path, "w")
        f.write("#!")
        f.write(d.getVar("base_bindir", True))
        f.write(l[4:])
        f.write(data)
        f.close()
        bb.warn("%s: hardcoded shebang in %s is replaced" % (package, path))
        return False


# Walk over all files in a directory and call func
def package_shebang_walk(pkg_path, package, d):
    for path in pkgfiles[package]:
        package_check_shebang(path, package, d)


# The PACKAGE FUNC to scan each package
python do_package_shebang () {
    import subprocess

    bb.note("DO PACKAGE shebang-rewrite")

    pkg = d.getVar('PN', True)

    # Scan the packages...
    pkgdest = d.getVar('PKGDEST', True)
    packages = d.getVar('PACKAGES', True)

    # no packages should be scanned
    if not packages:
        return

    for package in packages.split():
        bb.note("Processing package: %s" % package)
        pkg_path = "%s/%s" % (pkgdest, package)
        package_shebang_walk(pkg_path, package, d)

    bb.note("DONE with PACKAGE shebang-rewrite")
}


