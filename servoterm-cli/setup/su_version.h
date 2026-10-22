/*
 * Setup wizard: does the drive's firmware match this configurator?
 *
 * The wizard is built against one configuration interface (template names,
 * conf0 pins, the identification components): shared/setup_api.h's
 * STMBL_SETUP_API, which the drive prints in "about" ("setup api N"). A
 * drive without that line, or with another number, is refused: the wizard
 * would write a config that firmware doesn't understand. The git version
 * (git describe) is compared too; a difference with the same API is only a
 * warning.
 */
#pragma once

typedef struct {
  char product[32];  // "STMBL", "stmbl_host", ...
  int major, minor, patch;
  char git[48];      // git describe of the firmware
  int api;           // setup api, -1 when "about" doesn't print one
  int have_version;  // the "<product> vX.Y.Z <git>" line was found
} su_fw_t;

typedef enum {
  SU_FW_OK,
  SU_FW_GIT_DIFFERS,  // same API, other build: go on, but say so
  SU_FW_NO_API,       // firmware older than the wizard: update it
  SU_FW_API_DIFFERS,  // incompatible: update the firmware or the wizard
} su_fw_check_t;

// parse the text of "about" (the first version line is the F4's; the
// bootloader's and the HV board's follow it)
void su_fw_parse(const char *about, su_fw_t *fw);

// compare with what this wizard was built for (api, git; git may be "")
su_fw_check_t su_fw_check(const su_fw_t *fw, int api, const char *git);

// a sentence for the user about the result (static text + values in buf)
const char *su_fw_explain(su_fw_check_t r, const su_fw_t *fw, int api, const char *git, char *buf, int len);
