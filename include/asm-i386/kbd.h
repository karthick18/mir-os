/*Authors:
i) TABOS Team:
ii) A.R.Karthick
 Taken from TABOS and tweaked from MIR:
*/

#ifndef _KBD_H
#ifdef __KERNEL__
#define _KBD_H

#define BACKSPACE               0x8
#define KB_DATA_PORT		0x60	/* Keyboard data port		*/
#define KB_STATUS_PORT		0x64	/* Keyboard status port		*/

/* default ACK from keyboard following command */
#define KB_ACK                  0xFA
#define KB_STATUS_OBF		0x01	/* Keyboard output buffer full	*/
#define KB_STATUS_IBF		0x02	/* Keyboard input buffer full	*/
#define KB_REBOOT		0xFE	/* Reboot command		*/

#define KB_CMD_ENABLE		0xAE

#define KB_SHIFT_FLAG         0x100
#define KB_CTRL_FLAG          0x200
#define KB_ALT_FLAG           0x400
#define KB_LWIN_FLAG          0x800
#define KB_RWIN_FLAG          0x1000
#define KB_MENU_FLAG          0x2000
#define KB_SCROLOCK_FLAG      0x4000
#define KB_NUMLOCK_FLAG       0x8000
#define KB_CAPSLOCK_FLAG      0x10000
#define KB_INALTSEQ_FLAG      0x20000
#define KB_ACCENT1_FLAG       0x40000
#define KB_ACCENT2_FLAG       0x80000
#define KB_ACCENT3_FLAG       0x100000
#define KB_ACCENT4_FLAG       0x200000

#define LEDS_SHIFT            14

#define KEY_A                 1
#define KEY_B                 2
#define KEY_C                 3
#define KEY_D                 4
#define KEY_E                 5
#define KEY_F                 6
#define KEY_G                 7
#define KEY_H                 8
#define KEY_I                 9
#define KEY_J                 10
#define KEY_K                 11
#define KEY_L                 12
#define KEY_M                 13
#define KEY_N                 14
#define KEY_O                 15
#define KEY_P                 16
#define KEY_Q                 17
#define KEY_R                 18
#define KEY_S                 19
#define KEY_T                 20
#define KEY_U                 21
#define KEY_V                 22
#define KEY_W                 23
#define KEY_X                 24
#define KEY_Y                 25
#define KEY_Z                 26
#define KEY_0                 27
#define KEY_1                 28
#define KEY_2                 29
#define KEY_3                 30
#define KEY_4                 31
#define KEY_5                 32
#define KEY_6                 33
#define KEY_7                 34
#define KEY_8                 35
#define KEY_9                 36
#define KEY_0_PAD             37
#define KEY_1_PAD             38
#define KEY_2_PAD             39
#define KEY_3_PAD             40
#define KEY_4_PAD             41
#define KEY_5_PAD             42
#define KEY_6_PAD             43
#define KEY_7_PAD             44
#define KEY_8_PAD             45
#define KEY_9_PAD             46
#define KEY_F1                47
#define KEY_F2                48
#define KEY_F3                49
#define KEY_F4                50
#define KEY_F5                51
#define KEY_F6                52
#define KEY_F7                53
#define KEY_F8                54
#define KEY_F9                55
#define KEY_F10               56
#define KEY_F11               57
#define KEY_F12               58
#define KEY_ESC               59
#define KEY_TILDE             60
#define KEY_MINUS             61
#define KEY_EQUALS            62
#define KEY_BACKSPACE         63
#define KEY_TAB               64
#define KEY_OPENBRACE         65
#define KEY_CLOSEBRACE        66
#define KEY_ENTER             67
#define KEY_COLON             68
#define KEY_QUOTE             69
#define KEY_BACKSLASH         70
#define KEY_BACKSLASH2        71
#define KEY_COMMA             72
#define KEY_STOP              73
#define KEY_SLASH             74
#define KEY_SPACE             75
#define KEY_INSERT            76
#define KEY_DEL               77
#define KEY_HOME              78
#define KEY_END               79
#define KEY_PGUP              80
#define KEY_PGDN              81
#define KEY_LEFT              82
#define KEY_RIGHT             83
#define KEY_UP                84
#define KEY_DOWN              85
#define KEY_SLASH_PAD         86
#define KEY_ASTERISK          87
#define KEY_MINUS_PAD         88
#define KEY_PLUS_PAD          89
#define KEY_DEL_PAD           90
#define KEY_ENTER_PAD         91
#define KEY_PRTSCR            92
#define KEY_PAUSE             93
#define KEY_ABNT_C1           94
#define KEY_YEN               95
#define KEY_KANA              96
#define KEY_CONVERT           97
#define KEY_NOCONVERT         98
#define KEY_AT                99
#define KEY_CIRCUMFLEX        100
#define KEY_COLON2            101
#define KEY_KANJI             102

#define KEY_MODIFIERS         103

#define KEY_LSHIFT            103
#define KEY_RSHIFT            104
#define KEY_LCONTROL          105
#define KEY_RCONTROL          106
#define KEY_ALT               107
#define KEY_ALTGR             108
#define KEY_LWIN              109
#define KEY_RWIN              110
#define KEY_MENU              111
#define KEY_SCRLOCK           112
#define KEY_NUMLOCK           113
#define KEY_CAPSLOCK          114
#define FUNCTION_KEY          0xff

#define KEY_MAX               115
#define KB_MODIFIERS	( KB_SHIFT_FLAG | KB_CTRL_FLAG | KB_ALT_FLAG | \
			    KB_LWIN_FLAG | KB_RWIN_FLAG | KB_MENU_FLAG )
#define KB_LED_FLAGS	( KB_SCROLOCK_FLAG | KB_NUMLOCK_FLAG | \
			    KB_CAPSLOCK_FLAG )
#define KB_CTRL_ALT	( KB_CTRL_FLAG | KB_ALT_FLAG )

#define KB_FLAGS         (KB_MODIFIERS | KB_LED_FLAGS)

#define IS_KEY_LED(flag)   ( (flag) & KB_LED_FLAGS )
#define IS_KEY_EXTENDED(key)  ( (key) == 0xE0 || (key) == 0xE1 )
#define IS_KEY_FUNCTION(fkey,mycode,bitmask) ( ((mycode) == (fkey) ) && !( (bitmask) & KB_CTRL_FLAG) )
#define IS_KEY_F1(mycode,bitmask) IS_KEY_FUNCTION(KEY_F1,mycode,bitmask)
#define IS_KEY_F2(mycode,bitmask) IS_KEY_FUNCTION(KEY_F2,mycode,bitmask)
#define IS_KEY_F3(mycode,bitmask) IS_KEY_FUNCTION(KEY_F3,mycode,bitmask)
#define IS_KEY_F4(mycode,bitmask) IS_KEY_FUNCTION(KEY_F4,mycode,bitmask)

extern unsigned short *key_ascii_table;
extern unsigned short *key_capslock_table;
extern unsigned short *key_shift_table;
extern unsigned short *key_control_table;
extern int kbd_wait_write(unsigned short,unsigned char);
extern int kbd_wait_read(void);
extern int kbd_send_data(unsigned char);
extern unsigned char kbd_rw_ack(unsigned char);
extern void set_standard_keyboard(void);
extern void kbd_init(void);

#endif
#endif
