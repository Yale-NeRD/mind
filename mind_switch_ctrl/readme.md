# MIND control plan program for programmable switch
## Usage
- We assume that `$SDE` and `$SDE_INSTALL` are configured correctly during the installation of Intel P4 Studio (and software development environment; SDE).

- Add add the following to the `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/Makefile.am`
```diff
SUBDIRS = .
+ SUBDIRS += tna_disagg_switch
DIST_SUBDIRS = .
```

- Add current directory as `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples/tna_disagg_switch`

- Merge `Makefile.am` in this directory with the example Makefile.am provided by SDE

- In `Makefile.am`, update `path_to_mind_linux` as the path to *mind_linux* (not mind_ae)

- At `$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples`
  - In `tna_disagg_config.cpp`, update IP address, MAC address, and connected switch port number (*not front port number, but internal port number that can be checked in Intel's `bfshell` > `ucli` tool*)
  - Run `make` 

- To launch the compile program, use `./launch_disagg_switch.sh`
