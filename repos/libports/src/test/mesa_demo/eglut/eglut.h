/*
 * Copyright (C) 2010 LunarG Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 */

#ifndef EGLUT_H
#define EGLUT_H

/* used by eglutInitAPIMask */
enum {
   EGLUT_OPENGL_BIT     = 0x1,
   EGLUT_OPENGL_ES1_BIT = 0x2,
   EGLUT_OPENGL_ES2_BIT = 0x4,
   EGLUT_OPENVG_BIT     = 0x8
};

/* used by EGLUTspecialCB */
enum {
   /* function keys */
   EGLUT_KEY_F1,
   EGLUT_KEY_F2,
   EGLUT_KEY_F3,
   EGLUT_KEY_F4,
   EGLUT_KEY_F5,
   EGLUT_KEY_F6,
   EGLUT_KEY_F7,
   EGLUT_KEY_F8,
   EGLUT_KEY_F9,
   EGLUT_KEY_F10,
   EGLUT_KEY_F11,
   EGLUT_KEY_F12,

   /* directional keys */
   EGLUT_KEY_LEFT,
   EGLUT_KEY_UP,
   EGLUT_KEY_RIGHT,
   EGLUT_KEY_DOWN,
};

/* used by eglutGet */
enum {
   EGLUT_ELAPSED_TIME
};

typedef void (*EGLUTidleCB)(void);
typedef void (*EGLUTreshapeCB)(int, int);
typedef void (*EGLUTdisplayCB)(void);
typedef void (*EGLUTkeyboardCB)(unsigned char);
typedef void (*EGLUTspecialCB)(int);

void eglutInitAPIMask(int mask);
void eglutInitWindowSize(int width, int height);
void eglutInit(int argc, char **argv);

int eglutGet(int state);

void eglutIdleFunc(EGLUTidleCB func);
void eglutPostRedisplay(void);

void eglutMainLoop(void);

int eglutCreateWindow(const char *title);
void eglutDestroyWindow(int win);

int eglutGetWindowWidth(void);
int eglutGetWindowHeight(void);

void eglutDisplayFunc(EGLUTdisplayCB func);
void eglutReshapeFunc(EGLUTreshapeCB func);
void eglutKeyboardFunc(EGLUTkeyboardCB func);
void eglutSpecialFunc(EGLUTspecialCB func);

#endif /* EGLUT_H */
