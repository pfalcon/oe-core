# Unverified after rebase with conflicts
SUMMARY = "Tiny versions of many common UNIX utilities in a single small executable."
DESCRIPTION = "BusyBox combines tiny versions of many common UNIX utilities into a single small executable. It provides minimalist replacements for most of the utilities you usually find in GNU fileutils, shellutils, etc. The utilities in BusyBox generally have fewer options than their full-featured GNU cousins; however, the options that are included provide the expected functionality and behave very much like their GNU counterparts. BusyBox provides a fairly complete POSIX environment for any small or embedded system."
HOMEPAGE = "http://www.busybox.net"
BUGTRACKER = "https://bugs.busybox.net/"

DEPENDS += "kern-tools-native"

# bzip2 applet in busybox is based on lightly-modified bzip2 source
# the GPL is version 2 only
LICENSE = "GPLv2 & bzip2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=de10de48642ab74318e893a61105afbb"

SECTION = "base"

# Whether to split the suid apps into a seperate binary
BUSYBOX_SPLIT_SUID ?= "1"

export EXTRA_CFLAGS = "${CFLAGS}"
export EXTRA_LDFLAGS = "${LDFLAGS}"

PACKAGES =+ "${PN}-httpd ${PN}-udhcpd ${PN}-udhcpc ${PN}-syslog ${PN}-mdev ${PN}-hwclock"

FILES_${PN}-httpd = "${sysconfdir}/init.d/busybox-httpd /srv/www"
FILES_${PN}-syslog = "${sysconfdir}/init.d/syslog* ${sysconfdir}/syslog-startup.conf* ${systemd_unitdir}/system/syslog.service"
FILES_${PN}-mdev = "${sysconfdir}/init.d/mdev ${sysconfdir}/mdev.conf"
FILES_${PN}-udhcpd = "${sysconfdir}/init.d/busybox-udhcpd"
FILES_${PN}-udhcpc = "${sysconfdir}/udhcpc.d ${datadir}/udhcpc"
FILES_${PN}-hwclock = "${sysconfdir}/init.d/hwclock.sh"

INITSCRIPT_PACKAGES = "${PN}-httpd ${PN}-syslog ${PN}-udhcpd ${PN}-mdev ${PN}-hwclock"

INITSCRIPT_NAME_${PN}-httpd = "busybox-httpd"
INITSCRIPT_NAME_${PN}-hwclock = "hwclock.sh"
INITSCRIPT_NAME_${PN}-mdev = "mdev"
INITSCRIPT_PARAMS_${PN}-mdev = "start 03 S ."
INITSCRIPT_NAME_${PN}-syslog = "syslog"
INITSCRIPT_NAME_${PN}-udhcpd = "busybox-udhcpd" 

SYSTEMD_PACKAGES = "${PN}-syslog"
SYSTEMD_SERVICE_${PN}-syslog = "busybox-syslog.service"

CONFFILES_${PN}-syslog = "${sysconfdir}/syslog-startup.conf.${BPN}"
CONFFILES_${PN}-mdev = "${sysconfdir}/mdev.conf"

RRECOMMENDS_${PN} = "${PN}-syslog ${PN}-udhcpc"

inherit cml1 systemd update-rc.d ptest

# internal helper
def busybox_cfg(feature, features, tokens, cnf, rem):
	if type(tokens) == type(""):
		tokens = [tokens]
	rem.extend(['/^[# ]*' + token + '[ =]/d' for token in tokens])
	if type(features) == type([]) and feature in features:
		cnf.extend([token + '=y' for token in tokens])
	else:
		cnf.extend(['# ' + token + ' is not set' for token in tokens])

# Map distro features to config settings
def features_to_busybox_settings(d):
	cnf, rem = ([], [])
	distro_features = d.getVar('DISTRO_FEATURES', True).split()
	busybox_cfg('ipv6', distro_features, 'CONFIG_FEATURE_IPV6', cnf, rem)
	busybox_cfg('largefile', distro_features, 'CONFIG_LFS', cnf, rem)
	busybox_cfg('largefile', distro_features, 'CONFIG_FDISK_SUPPORT_LARGE_DISKS', cnf, rem)
	busybox_cfg('nls',  distro_features, 'CONFIG_LOCALE_SUPPORT', cnf, rem)
	busybox_cfg('ipv4', distro_features, 'CONFIG_FEATURE_IFUPDOWN_IPV4', cnf, rem)
	busybox_cfg('ipv6', distro_features, 'CONFIG_FEATURE_IFUPDOWN_IPV6', cnf, rem)
	busybox_cfg('wifi', distro_features, 'CONFIG_RFKILL', cnf, rem)
	busybox_cfg('bluetooth', distro_features, 'CONFIG_RFKILL', cnf, rem)
	return "\n".join(cnf), "\n".join(rem)

# X, Y = ${@features_to_uclibc_settings(d)}
# unfortunately doesn't seem to work with bitbake, workaround:
def features_to_busybox_conf(d):
	cnf, rem = features_to_busybox_settings(d)
	return cnf
def features_to_busybox_del(d):
	cnf, rem = features_to_busybox_settings(d)
	return rem

configmangle = '/CROSS_COMPILER_PREFIX/d; \
		/CONFIG_EXTRA_CFLAGS/d; \
		'
OE_FEATURES := "${@features_to_busybox_conf(d)}"
OE_DEL      := "${@features_to_busybox_del(d)}"
DO_IPv4 := "${@base_contains('DISTRO_FEATURES', 'ipv4', 1, 0, d)}"
DO_IPv6 := "${@base_contains('DISTRO_FEATURES', 'ipv6', 1, 0, d)}"

python () {
  if "${OE_DEL}":
    d.setVar('configmangle_append', "${OE_DEL}" + "\n")
  if "${OE_FEATURES}":
    d.setVar('configmangle_append',
                   "/^### DISTRO FEATURES$/a\\\n%s\n\n" %
                   ("\\n".join((d.expand("${OE_FEATURES}").split("\n")))))
  d.setVar('configmangle_append',
                 "/^### CROSS$/a\\\n%s\n" %
                  ("\\n".join(["CONFIG_CROSS_COMPILER_PREFIX=\"${TARGET_PREFIX}\"",
			       "CONFIG_EXTRA_CFLAGS=\"${CFLAGS}\" \"${HOST_CC_ARCH}\""
                        ])
                  ))
}

do_prepare_config () {
	sed -e 's#@DATADIR@#${datadir}#g' \
		< ${WORKDIR}/defconfig > ${S}/.config
	sed -i -e '/CONFIG_STATIC/d' .config
	echo "# CONFIG_STATIC is not set" >> .config
	for i in 'CROSS' 'DISTRO FEATURES'; do echo "### $i"; done >> \
		${S}/.config
	sed -i -e '${configmangle}' ${S}/.config
	if test ${DO_IPv4} -eq 0 && test ${DO_IPv6} -eq 0; then
	  # disable networking applets
	  mv ${S}/.config ${S}/.config.oe-tmp
	  awk 'BEGIN{net=0}
	  /^# Networking Utilities/{net=1}
	  /^#$/{if(net){net=net+1}}
	  {if(net==2&&$0 !~ /^#/&&$1){print("# "$1" is not set")}else{print}}' \
		  ${S}/.config.oe-tmp > ${S}/.config
	fi
}

# returns all the elements from the src uri that are .cfg files
def find_cfgs(d):
    sources=src_patches(d, True)
    sources_list=[]
    for s in sources:
        if s.endswith('.cfg'):
            sources_list.append(s)

    return sources_list

do_configure () {
	do_prepare_config
	merge_config.sh -m .config ${@" ".join(find_cfgs(d))}
	cml1_do_configure
}

do_compile() {
	unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS
	if [ "${BUSYBOX_SPLIT_SUID}" = "1" -a x`grep "CONFIG_FEATURE_INDIVIDUAL=y" .config` = x ]; then
	# split the .config into two parts, and make two busybox binaries
		cp .config .config.orig
		oe_runmake busybox.cfg.suid
		oe_runmake busybox.cfg.nosuid
		for i in `cat busybox.cfg.suid busybox.cfg.nosuid`; do
			echo "# $i is not set" >> .config.disable.apps
		done
		merge_config.sh -m .config.orig .config.disable.apps
		cp .config .config.nonapps
		for s in suid nosuid; do
			cat busybox.cfg.$s | while read item; do
				grep -w "$item" .config.orig
			done > .config.app.$s
			merge_config.sh -m .config.nonapps .config.app.$s
			oe_runmake busybox_unstripped
			mv busybox_unstripped busybox.$s
			oe_runmake busybox.links
			mv busybox.links busybox.links.$s
		done
		# copy .config.orig back to .config, because the install process may check this file
		cp .config.orig .config
		# cleanup
		rm .config.orig .config.app.suid .config.app.nosuid .config.disable.apps .config.nonapps
	else
		oe_runmake busybox_unstripped
		cp busybox_unstripped busybox
		oe_runmake busybox.links
	fi
}

do_install () {
	if [ "${prefix}" != "/usr" ]; then
		sed -i "s:^/usr/:${prefix}/:" busybox.links*
	fi
	if [ "${base_sbindir}" != "/sbin" ]; then
		sed -i "s:^/sbin/:${base_sbindir}/:" busybox.links*
	fi
	if [ "${base_bindir}" != "/bin" ]; then
		sed "s:^/bin/:${base_bindir}/:" busybox.links > busybox.links.new
		mv busybox.links.new busybox.links
	fi

	install -d ${D}${sysconfdir}/init.d

	if ! grep -q "CONFIG_FEATURE_INDIVIDUAL=y" ${B}/.config; then
		# Install /bin/busybox, and the /bin/sh link so the postinst script
		# can run. Let update-alternatives handle the rest.
		install -d ${D}${base_bindir}
		if [ "${BUSYBOX_SPLIT_SUID}" = "1" ]; then
			install -m 4755 ${B}/busybox.suid ${D}${base_bindir}
			install -m 0755 ${B}/busybox.nosuid ${D}${base_bindir}
			install -m 0644 ${S}/busybox.links.suid ${D}${sysconfdir}
			install -m 0644 ${S}/busybox.links.nosuid ${D}${sysconfdir}
			ln -sf busybox.nosuid ${D}${base_bindir}/sh
			# Keep a default busybox for people who want to invoke busybox directly.
			# This is also useful for the on device upgrade. Because we want
			# to use the busybox command in postinst.
			ln -sf busybox.nosuid ${D}${base_bindir}/busybox
		else
			if grep -q "CONFIG_FEATURE_SUID=y" ${B}/.config; then
				install -m 4755 ${B}/busybox ${D}${base_bindir}
			else
				install -m 0755 ${B}/busybox ${D}${base_bindir}
			fi
			install -m 0644 ${S}/busybox.links ${D}${sysconfdir}
			ln -sf busybox ${D}${base_bindir}/sh
			# We make this symlink here to eliminate the error when upgrading together
			# with busybox-syslog. Without this symlink, the opkg may think of the
			# busybox.nosuid as obsolete and remove it, resulting in dead links like
			# /bin/sed -> /bin/busybox.nosuid. This will make upgrading busybox-syslog fail.
			# This symlink will be safely deleted in postinst, thus no negative effect.
			ln -sf busybox ${D}${base_bindir}/busybox.nosuid
		fi
	else
		install -d ${D}${base_bindir} ${D}${base_sbindir}
		install -d ${D}${libdir} ${D}${bindir} ${D}${sbindir}
		cat busybox.links | while read FILE; do
			NAME=`basename "$FILE"`
			install -m 0755 "0_lib/$NAME" "${D}$FILE.${BPN}"
		done
		# add suid bit where needed
		for i in `grep -E "APPLET.*BB_SUID_((MAYBE|REQUIRE))" include/applets.h | grep -v _BB_SUID_DROP | cut -f 3 -d '(' | cut -f 1 -d ','`; do
			find ${D} -name $i.${BPN} -exec chmod a+s {} \;
		done
		install -m 0755 0_lib/libbusybox.so.${PV} ${D}${libdir}/libbusybox.so.${PV}
		ln -sf sh.${BPN} ${D}${base_bindir}/sh
		ln -sf ln.${BPN} ${D}${base_bindir}/ln
		ln -sf test.${BPN} ${D}${bindir}/test
		if [ -f ${D}/linuxrc.${BPN} ]; then
			mv ${D}/linuxrc.${BPN} ${D}/linuxrc
		fi
		install -m 0644 ${S}/busybox.links ${D}${sysconfdir}
	fi

	if grep -q "CONFIG_SYSLOGD=y" ${B}/.config; then
		install -m 0755 ${WORKDIR}/syslog ${D}${sysconfdir}/init.d/syslog.${BPN}
		install -m 644 ${WORKDIR}/syslog-startup.conf ${D}${sysconfdir}/syslog-startup.conf.${BPN}
	fi
	if grep "CONFIG_CROND=y" ${B}/.config; then
		install -m 0755 ${WORKDIR}/busybox-cron ${D}${sysconfdir}/init.d/
	fi
	if grep "CONFIG_HTTPD=y" ${B}/.config; then
		install -m 0755 ${WORKDIR}/busybox-httpd ${D}${sysconfdir}/init.d/
		install -d ${D}/srv/www
	fi
	if grep "CONFIG_UDHCPD=y" ${B}/.config; then
		install -m 0755 ${WORKDIR}/busybox-udhcpd ${D}${sysconfdir}/init.d/
	fi
	if grep "CONFIG_HWCLOCK=y" ${B}/.config; then
		install -m 0755 ${WORKDIR}/hwclock.sh ${D}${sysconfdir}/init.d/
	fi
	if grep "CONFIG_UDHCPC=y" ${B}/.config; then
		install -d ${D}${sysconfdir}/udhcpc.d
		install -d ${D}${datadir}/udhcpc
                install -m 0755 ${WORKDIR}/simple.script ${D}${sysconfdir}/udhcpc.d/50default
		install -m 0755 ${WORKDIR}/default.script ${D}${datadir}/udhcpc/default.script
	fi
	if grep "CONFIG_INETD=y" ${B}/.config; then
		install -m 0755 ${WORKDIR}/inetd ${D}${sysconfdir}/init.d/inetd.${BPN}
		sed -i "s:/usr/sbin/:${sbindir}/:" ${D}${sysconfdir}/init.d/inetd.${BPN}
		install -m 0644 ${WORKDIR}/inetd.conf ${D}${sysconfdir}/
	fi
        if grep "CONFIG_MDEV=y" ${B}/.config; then
               install -m 0755 ${WORKDIR}/mdev ${D}${sysconfdir}/init.d/mdev
               if grep "CONFIG_FEATURE_MDEV_CONF=y" ${B}/.config; then
                       install -m 644 ${WORKDIR}/mdev.conf ${D}${sysconfdir}/mdev.conf
               fi
	fi

    if ${@base_contains('DISTRO_FEATURES','systemd','true','false',d)}; then
        install -d ${D}${systemd_unitdir}/system
        sed 's,@base_sbindir@,${base_sbindir},g' < ${WORKDIR}/busybox-syslog.service.in \
            > ${D}${systemd_unitdir}/system/busybox-syslog.service
        sed 's,@base_sbindir@,${base_sbindir},g' < ${WORKDIR}/busybox-klogd.service.in \
            > ${D}${systemd_unitdir}/system/busybox-klogd.service

        ln -sf /dev/null ${D}${systemd_unitdir}/system/syslog.service
    fi

    # Remove the sysvinit specific configuration file for systemd systems to avoid confusion
    if ${@base_contains('DISTRO_FEATURES', 'sysvinit', 'false', 'true', d)}; then
	rm -f ${D}${sysconfdir}/syslog-startup.conf.${BPN}
    fi
}

inherit update-alternatives

ALTERNATIVE_PRIORITY = "50"

python () {
    if 'sysvinit' in d.getVar("DISTRO_FEATURES", True).split():
        pn = d.getVar('PN', True)
        d.appendVar('ALTERNATIVE_%s-syslog' % (pn), ' syslog-init')
        d.setVarFlag('ALTERNATIVE_LINK_NAME', 'syslog-init', '%s/init.d/syslog' % (d.getVar('sysconfdir', True)))
        d.setVarFlag('ALTERNATIVE_TARGET', 'syslog-init', '%s/init.d/syslog.%s' % (d.getVar('sysconfdir', True), d.getVar('BPN', True)))
        d.appendVar('ALTERNATIVE_%s-syslog' % (pn), ' syslog-startup-conf')
        d.setVarFlag('ALTERNATIVE_LINK_NAME', 'syslog-startup-conf', '%s/syslog-startup.conf' % (d.getVar('sysconfdir', True)))
        d.setVarFlag('ALTERNATIVE_TARGET', 'syslog-startup-conf', '%s/syslog-startup.conf.%s' % (d.getVar('sysconfdir', True), d.getVar('BPN', True)))
}
#ALTERNATIVE_TARGET = "${base_bindir}/busybox"

python do_package_prepend () {
    # We need to load the full set of busybox provides from the /etc/busybox.links
    # Use this to see the update-alternatives with the right information

    dvar = d.getVar('D', True)
    sysconfdir = d.getVar('sysconfdir', True)
    base_bindir = d.getVar('base_bindir', True)
    pn = d.getVar('PN', True)
    def set_alternative_vars(links, target):
        f = open('%s%s' % (dvar, links), 'r')
        for alt_link_name in f:
            alt_link_name = alt_link_name.strip()
            alt_name = os.path.basename(alt_link_name)
            # Match coreutils
            if alt_name == '[':
                alt_name = 'lbracket'
            d.appendVar('ALTERNATIVE_%s' % (pn), ' ' + alt_name)
            d.setVarFlag('ALTERNATIVE_LINK_NAME', alt_name, alt_link_name)
            if os.path.exists('%s%s' % (dvar, target)):
                d.setVarFlag('ALTERNATIVE_TARGET', alt_name, target)
        f.close()
        return

    if os.path.exists('%s/%s/busybox.links' % (dvar, sysconfdir)):
        set_alternative_vars("%s/busybox.links" % sysconfdir, "%s/busybox" % base_bindir)
    else:
        set_alternative_vars("%s/busybox.links.nosuid" % sysconfdir, "%s/busybox.nosuid" % base_bindir)
        set_alternative_vars("%s/busybox.links.suid" % sysconfdir, "%s/busybox.suid" % base_bindir)
}

pkg_postinst_${PN} () {
	# This part of code is dedicated to the on target upgrade problem.
	# It's known that if we don't make appropriate symlinks before update-alternatives calls,
	# there will be errors indicating missing commands such as 'sed'.
	# These symlinks will later be updated by update-alternatives calls.
	test -n 2 > /dev/null || alias test='busybox test'
	if test "x$D" = "x"; then
		# Remove busybox.nosuid if it's a symlink, because this situation indicates
		# that we're installing or upgrading to a one-binary busybox.
		if test -h /bin/busybox.nosuid; then
			rm -f /bin/busybox.nosuid
		fi
		for suffix in "" ".nosuid" ".suid"; do
			if test -e /etc/busybox.links$suffix; then
				while read link; do
					if test ! -e "$link"; then
						case "$link" in
                                                        # If symlink target is in the same dir,
                                                        # optimize and make relative symlink
                                                        ${base_bindir}*)
                                                                to="busybox$suffix"
                                                                ;;
                                                        # Otherwise, just make absolute symlink
                                                        *)
                                                                to="${base_bindir}/busybox$suffix"
                                                                ;;
						esac
						# we can use busybox here because even if we are using splitted busybox
						# we've made a symlink from /bin/busybox to /bin/busybox.nosuid.
						busybox rm -f $link
						busybox ln -s $to $link
					fi
				done < /etc/busybox.links$suffix
			fi
		done
	fi
}

pkg_prerm_${PN} () {
	# This is so you can make busybox commit suicide - removing busybox with no other packages
	# providing its files, this will make update-alternatives work, but the update-rc.d part
	# for syslog, httpd and/or udhcpd will fail if there is no other package providing sh
	tmpdir=`mktemp -d /tmp/busyboxrm-XXXXXX`
	ln -s ${base_bindir}/busybox $tmpdir/[
	ln -s ${base_bindir}/busybox $tmpdir/test
	ln -s ${base_bindir}/busybox $tmpdir/head
	ln -s ${base_bindir}/busybox $tmpdir/sh
	ln -s ${base_bindir}/busybox $tmpdir/basename
	ln -s ${base_bindir}/busybox $tmpdir/echo
	ln -s ${base_bindir}/busybox $tmpdir/mv
	ln -s ${base_bindir}/busybox $tmpdir/ln
	ln -s ${base_bindir}/busybox $tmpdir/dirname
	ln -s ${base_bindir}/busybox $tmpdir/rm
	ln -s ${base_bindir}/busybox $tmpdir/sed
	ln -s ${base_bindir}/busybox $tmpdir/sort
	ln -s ${base_bindir}/busybox $tmpdir/grep
	export PATH=$PATH:$tmpdir
}

pkg_prerm_${PN}-syslog () {
	# remove syslog
	if test "x$D" = "x"; then
		if test "$1" = "upgrade" -o "$1" = "remove"; then
			${sysconfdir}/init.d/syslog stop
		fi
	fi
}
