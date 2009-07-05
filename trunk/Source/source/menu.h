#ifndef _MENU_H_
#define _MENU_H_

/* Prototypes */
void Menu_Device(void);
void Menu_Format(void);
void Menu_Install(void);
void Menu_Remove(void);
void Menu_Boot(void);

void Menu_Loop(void);

void Menu_Options(void);
void Download_Cover(struct discHdr *header);
void Download_All(bool missing);

#endif

