/*
 * Setup wizard: the terminal front-end (servoterm-cli --wizard).
 *
 * Questions on stderr, answers from stdin. With an answers file ("key =
 * value" lines, the session's keys) the answers come from there; a question
 * the file doesn't answer falls back to the terminal, or, without one, to
 * the question's default (and fails when there is none). For hand actions
 * the file gives "done", or what to do in the simulator:
 *   fbcheck.turn = send hv0.sim_turn = 19; wait 5
 * ("send" a console line, "wait" seconds while reading the drive).
 */
#pragma once
#include "su_ui.h"

typedef struct su_cli su_cli_t;

su_cli_t *su_cli_new(const char *answers_path, int verbose);  // NULL: answers file unreadable
void su_cli_free(su_cli_t *c);
su_ui_t *su_cli_ui(su_cli_t *c);
