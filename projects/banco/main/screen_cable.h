// SPDX-License-Identifier: Apache-2.0
/* Cable voltage drop: from current, line length and the allowed drop, find the
 * smallest standard conductor section. Resistive drop only (cos φ = 1), copper
 * or aluminium at 70 °C, single-phase 230 V or three-phase 400 V. */
#pragma once
#include "calc.h"
#include "ui_widgets.h"

enum { CABLE_FIELD_CURRENT, CABLE_FIELD_LENGTH, CABLE_FIELD_LIMIT, CABLE_FIELD_COUNT };

/* View lines: 0 the section found ("2,5 mm²", "-", or "OLTRE 240 mm²"),
 * 1 its drop ("8,64 V   3,76 %"), 2 the hint or error,
 * 3..5 the typed current, length and limit. */
enum { CABLE_LINE_SECTION, CABLE_LINE_DROP, CABLE_LINE_HINT, CABLE_LINE_ENTRIES };

typedef struct {
    calc_entry_t entry[CABLE_FIELD_COUNT];
    int selected;
    cable_material_t material;
    cable_system_t system;
    bool no_section;                /* Valid input, but no standard section keeps within the limit. */
    ui_lines_t view;
} cable_state_t;

void cable_init(cable_state_t *state);
/* Rebuild the texts that depend on the language, keeping the entered values. */
void cable_relocalize(cable_state_t *state);
void cable_tap(cable_state_t *state, int32_t x, int32_t y);
void cable_draw(const cable_state_t *state, const ui_pointer_t *pointer);
