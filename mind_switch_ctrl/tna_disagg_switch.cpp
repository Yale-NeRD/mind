#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_common.h>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <getopt.h>
#include <unistd.h>
#include "tna_disagg_bfrt.hpp"

#include <csignal>

#ifdef __cplusplus
extern "C" {
#endif
#include <bf_switchd/bf_switchd.h>
#ifdef __cplusplus
}
#endif

static void parse_options(bf_switchd_context_t *switchd_ctx,
                          int argc,
                          char **argv) {
  int option_index = 0;
  enum opts {
    OPT_INSTALLDIR = 1,
    OPT_CONFFILE,
  };
  static struct option options[] = {
      {"help", no_argument, 0, 'h'},
      {"install-dir", required_argument, 0, OPT_INSTALLDIR},
      {"conf-file", required_argument, 0, OPT_CONFFILE}};

  while (1) {
    int c = getopt_long(argc, argv, "h", options, &option_index);

    if (c == -1) {
      break;
    }
    switch (c) {
      case OPT_INSTALLDIR:
        switchd_ctx->install_dir = strdup(optarg);
        printf("Install Dir: %s\n", switchd_ctx->install_dir);
        break;
      case OPT_CONFFILE:
        switchd_ctx->conf_file = strdup(optarg);
        printf("Conf-file : %s\n", switchd_ctx->conf_file);
        break;
      case 'h':
      case '?':
        printf("tna_exact_match \n");
        printf(
            "Usage : tna_exact_match --install-dir <path to where the SDE is "
            "installed> --conf-file <full path to the conf file "
            "(tna_exact_match.conf)\n");
        exit(c == 'h' ? 0 : 1);
        break;
      default:
        printf("Invalid option\n");
        exit(0);
        break;
    }
  }
  if (switchd_ctx->install_dir == NULL) {
    printf("ERROR : --install-dir must be specified\n");
    exit(0);
  }

  if (switchd_ctx->conf_file == NULL) {
    printf("ERROR : --conf-file must be specified\n");
    exit(0);
  }
}

extern "C" 
{
  extern int network_server_init(void);
  extern void network_server_exit(void);
}

void term_signal_handler(int signum)
{
  network_server_exit();
  printf("Signal received(%d): terminating process...\n", signum);
  exit(signum);
}

int main(int argc, char **argv) {
  // int main(void) {
  bf_switchd_context_t *switchd_ctx;
  printf("Inside tna_disagg_switch.cpp\n");
  if ((switchd_ctx = (bf_switchd_context_t *)calloc(
          1, sizeof(bf_switchd_context_t))) == NULL)
  {
    printf("Cannot Allocate switchd context\n");
    exit(1);
  }
  parse_options(switchd_ctx, argc, argv);
  switchd_ctx->running_in_background = true;
  switchd_ctx->kernel_pkt = true; // PCIe CPU port
  bf_status_t status = bf_switchd_lib_init(switchd_ctx);

  // Do initial set up
  bfrt::tna_disagg_switch::setUp();
  // Do table level set up
  bfrt::tna_disagg_switch::tableSetUp();
  bfrt::tna_disagg_switch::table_insert_init_data();

  // Indicator for running TCP server
  signal(SIGINT, term_signal_handler);
  network_server_init();
  printf("TCP/UDP server started...\n");

  // Initial script here
  system("/home/sslee/sde/bf-sde-9.2.0/install/bin/bfshell -f enable_ports.bfshell");

  while (true)
  {
    sleep(1);
  }

  // Free resources and exit
  if (switchd_ctx)
    cfree(switchd_ctx);
  printf("Main bf_switchd - tna_disagg_switch is terminated\n");
  return status;
}
