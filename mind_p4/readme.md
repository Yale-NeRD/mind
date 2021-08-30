# MIND's P4 program
## Usage (how to build and run)

- We assume that `$SDE` and `$SDE_INSTALL` are configured correctly during the installation of Intel P4 Studio (and software development environment; SDE).

- Place this directory at `$SDE/pkgsrc/p4-examples/p4_16_programs/tna_disagg_switch`
  - Note: here, `mind_p4` becomes `tna_disagg_switch`

- `cd $SDE/pkgsrc/p4-examples/p4_16_programs`
  - Move `$SDE/pkgsrc/p4-examples/p4_16_programs/tna_disagg_switch/configure_p4_build.sh` file to `$SDE/pkgsrc/p4-examples/p4_16_programs/configure_p4_build.sh`
  - Configure build system by: `sh configure_p4_build.sh tna_disagg_switch`
  - Build P4 program by: `make && make install`
