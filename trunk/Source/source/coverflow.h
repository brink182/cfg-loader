#ifndef _COVERFLOW_H_
#define _COVERFLOW_H_


extern Mtx GXmodelView2D;

//------prototypes------//
/**
 * Initilaizes all the common cover images (top, side, front, back)
 *  @return void
 */
void Coverflow_Grx_Init(void);

/**
 * Frees image resources
 *  @return void
 */
void Coverflow_Close(void);

/**
 * Initializes the cover coordinates based on the currently selected theme
 *  @param coverCount total number of games
 *  @param selectedCoverIndex index of the currently selected cover
 *  @param themeChange boolean to determine if a coverflow gui theme change is being performed
 *  @return int representing the index of the currently selected cover
 */
int Coverflow_initCoverObjects(int coverCount, int selectedCoverIndex, bool themeChange);

/**
 * Method that repaints the static covers.  This gets called when nothing is really going on (no rotating)
 *
 *  @param *ir pointer to current wiimote data
 *  @param num_side_covers number of side covers to draw
 *  @param coverCount total number of games
 *  @param draw_title boolean to determine if the game title should be drawn
 *  @return int representing the index of the currently selected cover
 */
int Coverflow_drawCovers(ir_t *ir, int num_side_covers, int coverCount, bool draw_title);

/**
 * Method that flips the center cover 180 degrees from it's current position to either a fullscreen view (on it's back cover)
 *  or back to where it should be (on it's front cover).  If showBackCover is true, it will flip to the back.
 *  @param showBackCover boolean to tell if back cover should be shown
 *  @param TypeUsed if 1 then wiimote if 2 then dpad if 0 then just flip it with no checks
 *  @return int representing the index of the currently selected cover
 */
int Coverflow_flip_cover_to_back(bool showBackCover, int TypeUsed);

/**
 * Checks the current wiimote params to determine if an action needs to be performed
 *  e.g. rotate the center cover, scroll right/left, etc.
 *
 *  @param *ir IR info from the WiiMote
 *  @param coverCount total number of games
 *  @return int representing the index of the currently selected cover
 */
int Coverflow_process_wiimote_events(ir_t *ir, int);

/**
 * Method used to inititate a new coverflow transition (rotate right/left, spin, etc).
 *
 *  @param trans_type int representing the transition type to initiate (rotate right/left, spin, etc)
 *  @param speed int representing the transition speed.  Set to 0 for the "default" slow speed.
 *  @param coverCount total number of games
 *  @param speed_rampup bool representing if each iteration of the transition should increase in speed
 *  @return int representing the index of the currently selected cover
 */
int Coverflow_init_transition(int trans_type, int speed, int coverCount, bool speed_rampup);

#endif
