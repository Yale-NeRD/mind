# MIND control plan program for programmable switch
## Usage
- We assume that `$SDE` and `$SDE_INSTALL` are configured correctly during the installation of Intel P4 Studio (and software development environment; SDE).
  - We use `mind` to indicate the root directory of this repository.
  - Due to NDA, we tried to include a minimal set of source codes that is directly written by ourselves.

- Add add the following to the `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/Makefile.am`
  ```diff
  SUBDIRS = .
  + SUBDIRS += tna_disagg_switch
  ```
  - If needed, disable other programs by commenting out the following lines:
    - `bin_PROGRAMS = \` and the follwing lines about program names
    - `tna_exact_match_SOURCES` and the following lines about `tna_exct_match` and other example programs

- Add current directory as `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/tna_disagg_switch`
  - Set up `Makefile.am`
    - Copy missing codes from Intel's example at `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/Makefile.am` and merge them with `Makefile.am` in this directory.
      - For example, copy all the missed flags such as `ACLOCAL_AMFLAGS`, `common_libs`, and `bin_PROGRAMS` from `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/Makefile.am` to `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/tna_disagg_switch/Makefile.am`
    - In `Makefile.am`, update `path_to_mind_linux` as the path to `mind/mind_linux` (*not* `mind`)
  - In the file, `$SDE/pkgsrc/bf-drivers/configure.ac`, add `bf_switchd/bfrt_examples/tna_disagg_switch/Makefile` at the end of the list of other files inside `AC_CONFIG_FILES`
    ```diff
    AC_CONFIG_FILES([Makefile
    (omitted ...)
    bf_switchd/bfrt_examples/Makefile
    + bf_switchd/bfrt_examples/tna_disagg_switch/Makefile])
    ```
- Inside `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples`
  - In `tna_disagg_config.cpp`, update IP address, MAC address, and connected switch port number (*not front port number, but internal port number that can be checked in Intel's `bfshell` > `ucli` tool*).

- Inside `$SDE/pkgsrc/bf-drivers/`
  - Configure driver by `./configure --prefix=$SDE_INSTALL --enable-grpc --enable-thrift --host=x86_64-linux-gnu`
  - Run `make` and `make install`

- To launch the compile program, use `./launch_disagg_switch.sh`
