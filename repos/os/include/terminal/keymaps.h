/*
 * \brief  Key mappings of scancodes to characters
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__KEYMAPS_H_
#define _TERMINAL__KEYMAPS_H_

namespace Terminal {

	enum {
		BS  =   8,
		ESC =  27,
		TAB =   9,
		LF  =  10,
		UE  = 252,   /* 'ü' */
		AE  = 228,   /* 'ä' */
		OE  = 246,   /* 'ö' */
		PAR = 167,   /* '§' */
		DEG = 176,   /* '°' */
		SS  = 223,   /* 'ß' */
	};


	static unsigned char usenglish_keymap[128] = {
		 0 ,ESC,'1','2','3','4','5','6','7','8','9','0','-','=', BS,TAB,
		'q','w','e','r','t','y','u','i','o','p','[',']', LF, 0 ,'a','s',
		'd','f','g','h','j','k','l',';','\'','`', 0, '\\' ,'z','x','c','v',
		'b','n','m',',','.','/', 0 , 0 , 0 ,' ', 0 , 0 , 0 , 0 , 0 , 0 ,
		 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'7','8','9','-','4','5','6','+','1',
		'2','3','0',',', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		 LF, 0 ,'/', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	};


	/**
	 * Mapping from ASCII value to another ASCII value when shift is pressed
	 *
	 * The table does not contain mappings for control characters. The table
	 * entry 0 corresponds to ASCII value 32.
	 */
	static unsigned char usenglish_shift[256 - 32] = {
		/*  32 */ ' ', 0 , 0,  0 , 0 , 0 , 0 ,'"', 0 , 0 , 0 , 0 ,'<','_','>','?',
		/*  48 */ ')','!','@','#','$','%','^','&','*','(', 0 ,':', 0 ,'+', 0 , 0 ,
		/*  64 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/*  80 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'{','|','}', 0 , 0 ,
		/*  96 */ '~','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
		/* 112 */ 'P','Q','R','S','T','U','V','W','X','Y','Z', 0 ,'\\', 0 , 0 , 0 ,
	};


	static unsigned char german_keymap[128] = {
		 0 ,ESC,'1','2','3','4','5','6','7','8','9','0', SS, 39, BS,TAB,
		'q','w','e','r','t','z','u','i','o','p', UE,'+', LF, 0 ,'a','s',
		'd','f','g','h','j','k','l', OE, AE,'^', 0 ,'#','y','x','c','v',
		'b','n','m',',','.','-', 0 ,'*', 0 ,' ', 0 , 0 , 0 , 0 , 0 , 0 ,
		 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'7','8','9','-','4','5','6','+','1',
		'2','3','0',',', 0 , 0 ,'<', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		 LF, 0 ,'/', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	};


	/**
	 * Mapping from ASCII value to another ASCII value when shift is pressed
	 *
	 * The table does not contain mappings for control characters. The table
	 * entry 0 corresponds to ASCII value 32.
	 */
	static unsigned char german_shift[256 - 32] = {
		/*  32 */ ' ', 0 , 0, 39 , 0 , 0 , 0 ,'`', 0 , 0 , 0 ,'*',';','_',':', 0 ,
		/*  48 */ '=','!','"',PAR,'$','%','&','/','(',')', 0 , 0 ,'>', 0 , 0 , 0 ,
		/*  64 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/*  80 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,DEG, 0 ,
		/*  96 */  0 ,'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
		/* 112 */ 'P','Q','R','S','T','U','V','W','X','Y','Z', 0 , 0 , 0 , 0 , 0 ,
		/* 128 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 144 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 160 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 176 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 192 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 208 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'?',
	};


	/**
	 * Mapping from ASCII value to another ASCII value when altgr is pressed
	 */
	static unsigned char german_altgr[256 - 32] = {
		/*  32 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'~', 0 , 0 , 0 , 0 ,
		/*  48 */'}', 0 ,178,179, 0 , 0 , 0 ,'{','[',']', 0 , 0 ,'|', 0 , 0 , 0 ,
		/*  64 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/*  80 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/*  96 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 112 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 128 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 144 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 160 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 176 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 192 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
		/* 208 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'\\',
	};


	/**
	 * Mapping from ASCII value to value reported when control is pressed
	 *
	 * The table starts with ASCII value 32.
	 */
	static unsigned char control[256 - 32] = {
		/*  32 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		/*  48 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		/*  64 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		/*  80 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		/*  96 */  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		/* 112 */ 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,  0,
	};
}

#endif /* _TERMINAL__KEYMAPS_H_ */
