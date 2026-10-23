#pragma once

#define VERSION_MAJOR 0
#define VERSION_MINOR 9
#define VERSION_PATCH 18

// The firmware's version as one number, counted up: the LV (F4) and HV (F3)
// firmware carry it, the HV board reports the one it runs, the drive refuses
// to enable when they differ, and the console (webui/mill) expects the one of
// its own checkout. Count it up with every firmware change a config, the HV
// board or the console must know about; a change to the config's format also
// gets a patch in conf/patches/ (docs/firmware_versions.md).
#define STMBL_FW_VERSION 1
#define STMBL_FW_MAGIC 0x56574653u  // "SFWV": version_info_t's fw_version is there
