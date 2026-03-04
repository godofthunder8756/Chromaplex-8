/*
 * cx8_home.h — Chromaplex 8 Home Screen & Cart Browser
 *
 * Displays the system splash, scans a carts/ folder for .lua and
 * .cx8 cartridges, and lets the user browse + launch them.
 */

#ifndef CX8_HOME_H
#define CX8_HOME_H

#include "cx8.h"

#define CX8_HOME_MAX_CARTS  128
#define CX8_HOME_NAME_LEN   64
#define CX8_HOME_PATH_LEN   512

/* Entry in the cart list */
typedef struct {
    char name[CX8_HOME_NAME_LEN];     /* display name (from title or filename) */
    char author[CX8_HOME_NAME_LEN];
    char path[CX8_HOME_PATH_LEN];     /* full file path */
} cx8_home_entry_t;

/* Result from the home screen */
typedef enum {
    CX8_HOME_NONE,      /* still browsing              */
    CX8_HOME_RUN,       /* user selected a cart to run  */
    CX8_HOME_EDIT,      /* user wants to edit a cart    */
    CX8_HOME_NEW,       /* user wants a new blank cart  */
    CX8_HOME_QUIT,      /* user wants to quit           */
} cx8_home_result_t;

/* Initialise home screen: scan the given directory for carts */
void cx8_home_init(const char *carts_dir);

/* Process one frame: returns result action */
cx8_home_result_t cx8_home_update(void);

/* Draw the home screen to VRAM */
void cx8_home_draw(void);

/* Get the currently selected cart path (valid after RUN or EDIT) */
const char *cx8_home_selected_path(void);

/* Get selected index */
int cx8_home_selected_index(void);

/* Get entry count */
int cx8_home_entry_count(void);

/* Get an entry by index */
const cx8_home_entry_t *cx8_home_get_entry(int idx);

#endif /* CX8_HOME_H */
