require gcc-configure-common.inc

CXXFLAGS := "${@oe_filter_out('-fvisibility-inlines-hidden', '${CXXFLAGS}', d)}"

EXTRA_OECONF_PATHS = " \
    --with-gxx-include-dir=${includedir}/c++/ \
    --with-sysroot=${STAGING_DIR_TARGET} \
    --with-build-sysroot=${STAGING_DIR_TARGET}"

ARCH_FLAGS_FOR_TARGET += "-isystem${STAGING_INCDIR}"

EXTRA_OECONF += "--disable-libunwind-exceptions"
EXTRA_OECONF_append_linuxstdbase = " --enable-clocale=gnu"

RUNTIMETARGET = "libssp libstdc++-v3 libgomp"
#  ?
# libiberty
# libmudflap
# libgfortran

do_configure () {
	export CXX="${CXX} -nostdinc++ -nostdlib++"
	mtarget=`echo ${MULTIMACH_TARGET_SYS} | sed -e s#-${SDKPKGSUFFIX}##`
	target=`echo ${TARGET_SYS} | sed -e s#-${SDKPKGSUFFIX}##`
	cp -fpPR ${STAGING_INCDIR_NATIVE}/gcc-build-internal-$mtarget/* ${B}
	for d in libgcc ${RUNTIMETARGET}; do
		echo "Configuring $d"
		rm -rf ${B}/$target/$d/
		mkdir -p ${B}/$target/$d/
		cd ${B}/$target/$d/
		chmod a+x ${S}/$d/configure
		${S}/$d/configure ${CONFIGUREOPTS} ${EXTRA_OECONF}
	done
}

do_compile () {
	target=`echo ${TARGET_SYS} | sed -e s#-${SDKPKGSUFFIX}##`
	for d in libgcc ${RUNTIMETARGET}; do
		cd ${B}/$target/$d/
		oe_runmake MULTIBUILDTOP=${B}/$target/$d/
	done
}

do_install () {
	target=`echo ${TARGET_SYS} | sed -e s#-${SDKPKGSUFFIX}##`
	for d in ${RUNTIMETARGET}; do
		cd ${B}/$target/$d/
		oe_runmake 'DESTDIR=${D}' MULTIBUILDTOP=${B}/$target/$d/ install
	done
	rm -rf ${D}${infodir}/libgomp.info ${D}${infodir}/dir
	if [ -d ${D}${libdir}/gcc/${TARGET_SYS}/${BINV}/finclude ]; then
		rmdir --ignore-fail-on-non-empty -p ${D}${libdir}/gcc/${TARGET_SYS}/${BINV}/finclude
	fi
	if [ -d ${D}${infodir} ]; then
		rmdir --ignore-fail-on-non-empty -p ${D}${infodir}
	fi
	chown -R root:root ${D}
}

INHIBIT_DEFAULT_DEPS = "1"
DEPENDS = "virtual/${TARGET_PREFIX}gcc virtual/${TARGET_PREFIX}g++ libgcc"
PROVIDES = "virtual/${TARGET_PREFIX}compilerlibs"

BBCLASSEXTEND = "nativesdk"

PACKAGES = "\
  ${PN}-dbg \
  libstdc++ \
  libstdc++-precompile-dev \
  libstdc++-dev \
  libstdc++-staticdev \
  libg2c \
  libg2c-dev \
  libssp \
  libssp-dev \
  libssp-staticdev \
  libgfortran \
  libgfortran-dev \
  libmudflap \
  libmudflap-dev \
  libgomp \
  libgomp-dev \
  libgomp-staticdev \
"
# The base package doesn't exist, so we clear the recommends.
RRECOMMENDS_${PN}-dbg = ""

# include python debugging scripts
FILES_${PN}-dbg += "\
  ${libdir}/libstdc++.so.*-gdb.py \
  ${datadir}/gcc-${BINV}/python/libstdcxx"

FILES_libg2c = "${target_libdir}/libg2c.so.*"
FILES_libg2c-dev = "\
  ${libdir}/libg2c.so \
  ${libdir}/libg2c.a \
  ${libdir}/libfrtbegin.a"

FILES_libstdc++ = "${libdir}/libstdc++.so.*"
FILES_libstdc++-dev = "\
  ${includedir}/c++/ \
  ${libdir}/libstdc++.so \
  ${libdir}/libstdc++.la \
  ${libdir}/libsupc++.la"
FILES_libstdc++-staticdev = "\
  ${libdir}/libstdc++.a \
  ${libdir}/libsupc++.a"

FILES_libstdc++-precompile-dev = "${includedir}/c++/${TARGET_SYS}/bits/*.gch"

FILES_libssp = "${libdir}/libssp.so.*"
FILES_libssp-dev = " \
  ${libdir}/libssp*.so \
  ${libdir}/libssp*_nonshared.a \
  ${libdir}/libssp*.la \
  ${libdir}/gcc/${TARGET_SYS}/${BINV}/include/ssp"
FILES_libssp-staticdev = " \
  ${libdir}/libssp*.a"

FILES_libgfortran = "${libdir}/libgfortran.so.*"
FILES_libgfortran-dev = " \
  ${libdir}/libgfortran.a \
  ${libdir}/libgfortran.so \
  ${libdir}/libgfortranbegin.a"

FILES_libmudflap = "${libdir}/libmudflap*.so.*"
FILES_libmudflap-dev = "\
  ${libdir}/libmudflap*.so \
  ${libdir}/libmudflap*.a \
  ${libdir}/libmudflap*.la"

FILES_libgomp = "${libdir}/libgomp*${SOLIBS}"
FILES_libgomp-dev = "\
  ${libdir}/libgomp*${SOLIBSDEV} \
  ${libdir}/libgomp*.la \
  ${libdir}/libgomp.spec \
  ${libdir}/gcc/${TARGET_SYS}/${BINV}/include/omp.h \
  "
FILES_libgomp-staticdev = "\
  ${libdir}/libgomp*.a \
  "

do_package_write_ipk[depends] += "virtual/${MLPREFIX}libc:do_packagedata"
do_package_write_deb[depends] += "virtual/${MLPREFIX}libc:do_packagedata"
do_package_write_rpm[depends] += "virtual/${MLPREFIX}libc:do_packagedata"
