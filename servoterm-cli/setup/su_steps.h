/*
 * Setup wizard: the steps and the engine that runs them.
 *
 * A step asks through the UI (su_ui.h), talks to the drive (su_link.h) and
 * keeps everything it learns in the session state (su_state.h). After each
 * finished step the engine marks it ("done.<id> = 1") and saves the
 * session, so a stopped wizard resumes at the first unfinished step. A GUI
 * can run the whole wizard (su_wizard_run) or single steps (su_wizard_step),
 * show the state, and build the config text at any point (su_build_config).
 *
 * Steps, in order:
 *   version     the drive's firmware matches this wizard (setup api)
 *   backup      the drive's current config saved to a file
 *   safety      what the wizard will do; e-stop at hand
 *   supply      mains / DC bus, brake chopper, power rating
 *   motor       nameplate: currents, speed, torque, holding brake
 *   feedback    the motor feedback type
 *   wiring      wiring help, then the user wires it
 *   base        a first config (no command link) written, drive reset, checked
 *   fbcheck     the shaft turned by hand: resolution (index) and direction
 *   gate        before anything moves: motor free, travel allowed
 *   electrical  idpmsm: r, l, pole pairs, commutation offset, phase order, psi
 *   mechanical  idm: inertia, damping, friction
 *   tuning      ids (id_pid): position / velocity loop gains
 *   limits      current, torque and speed limits; the thermal model
 *   link        LinuxCNC smart-serial, CAN, or none; units per revolution
 *   verify      the final config: hold, small steps, no fault, errors small
 *   save        a copy of the config and a report
 */
#pragma once
#include "su_link.h"
#include "su_state.h"
#include "su_ui.h"

typedef enum { SU_OK = 0, SU_FAIL = 1, SU_ABORTED = 2 } su_result_t;

typedef struct {
  su_link_t *link;
  su_ui_t *ui;
  su_state_t *st;
  const char *session;  // saved after every step; NULL: not saved
  const char *out_dir;  // backups, the final config, the report ("." if NULL)
  int api;              // STMBL_SETUP_API this wizard is built for
  const char *git;      // its git version ("" if unknown)
  double time_scale;    // drive time per wall time (1; the simulator's --speed): shortens waits
} su_wizard_t;

typedef struct {
  const char *id;
  const char *title;
  su_result_t (*run)(su_wizard_t *w);
} su_step_t;

extern const su_step_t su_steps[];
extern const int su_nsteps;

// run every step not yet done, in order
su_result_t su_wizard_run(su_wizard_t *w);
// run one step (and mark it done)
su_result_t su_wizard_step(su_wizard_t *w, const char *id);
// forget step id and everything after it (to redo them)
void su_wizard_redo(su_wizard_t *w, const char *id);

// the config for what the state knows; final adds the command link
char *su_build_config(const su_state_t *st, int final);
