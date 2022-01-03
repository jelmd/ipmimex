# ipmimex

ipmimex is a *m*etrics *ex*porter for platform services providing an "Intelligent Platform Management Interface" (*IPMI*) version <= 2.0. *ipmimex* uses this protocol to collect desired data and optionally exposes them via HTTP in [Prometheuse exposition format](https://prometheus.io/docs/instrumenting/exposition_formats/) using the endpoint URL http://_hostname:9290_/metrics (port and IP are customizable of course) and thus visualized e.g. using [Grafana](https://grafana.com/), [Netdata](https://www.netdata.cloud/), or [Zabbix](https://www.zabbix.com/).

Basically *ipmimex* is able to retrieve and expose all data from IPMI services (e.g. running on a *B*aseboard *M*anagement *C*ontroller (BMC)) you may query manually using `ipmitool sdr type {Temperature|Voltage|Fan}` and `ipmitool dcmi power reading`. But instead of the fork/exec nightmare seen on other IPMI metrics exporters (and their inefficient, slow data processing/resource usage) *ipmimex* is a real daemon written in **C**, which caches as much data as possible and talks directly to the IPMI service - per default via */dev/ipmi0* (the OpenIPMI interface of modern Linux kernels) or */dev/bmc* (Solaris 11).

## KISS

Since efficiency, size and simplicity of the utility is one of its main goals, OEM specific records/data get ignored (haven't seen yet any OEM specific data exposed via IPMI, which are worth to monitor). Beside [libprom](https://github.com/jelmd/libprom) to handle some prometheus (PROM) related stuff and [libmicrohttpd](https://github.com/Karlson2k/libmicrohttpd) to provide http access, no 3rd party libraries, tools, etc. are used. Last but not least there is intentionally no IPMI LAN[+] support to query e.g. remote services. The basic idea is to run *ipmimex* as a local service on the machine to monitor and use OS tools and services (firewall, http proxy, VictoriaMetrics vmagent, and the like) to control access to exposed data.


## Requirements

- [libprom](https://github.com/jelmd/libprom)
- [libmicrohttpd](https://github.com/Karlson2k/libmicrohttpd)


## Build

Adjust the **Makefile** if needed, optionally set related environment variables
(e.g. `export USE_CC=gcc`) and run GNU **make**.


## Repo

The official repository for *ipmimex* is https://github.com/jelmd/ipmimex .
If you need some new features (or bug fixes), please feel free to create an
issue there using https://github.com/jelmd/ipmimex/issues .


## Versioning

*ipmimex* follows the basic idea of semantic versioning, but having the real world
in mind. Therefore official releases have always THREE numbers (A.B.C), not
more and not less! For nightly, alpha, beta, RC builds, etc. a *.0* and
possibly more dot separated digits will be append, so that one is always able
to overwrite this one by using a 4th digit > 0.


## License

[CDDL 1.1](https://spdx.org/licenses/CDDL-1.1.html)


## Ubuntu packages
Ubuntu packages for libprom and ipmimex can be found via https://pkg.cs.ovgu.de/LNF/linux/ubuntu/ (search for libprom*.deb and ipmimex*.deb). libmicrohttpd gets provided by Ubuntu itself, so using the vendor package is recommended (for Ubuntu 20.04 it is named libmicrohttpd12). Related packages with header sources files are named libprom-dev.deb and libmicrohttpd-dev.deb.
