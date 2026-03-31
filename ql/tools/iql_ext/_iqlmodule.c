/*
 * _iqlmodule.c — CPython extension wrapping the iQL emulator core
 *
 * Exposes the Sinclair QL emulator as a Python module for embedding
 * in QL Studio (sprite_editor.py).
 *
 * Architecture:
 *   - QLStart() runs in a background thread (blocks until QLdone=1)
 *   - Python calls iql_tick() periodically from Tkinter's after() loop
 *     which fires QLTimer() to advance emulation
 *   - pixel_buffer (512x256 RGBA) is read directly for display
 *   - Keyboard input forwarded via QLRBSendEvent()
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include <pthread.h>

#include "QL_68000.h"
#include "QL_screen.h"
#include "rb_screen.h"
#include "rb_virtual_keys.h"

/* iQL emulator public API */
extern int QLStart(void);
extern void QLTimer(void);
extern void QLRestart(void);
extern void QLStop(void);
extern void QLSetSpeed(int ms);

/* Our platform stub */
extern void iql_set_system_path(const char *path);

/* Screen buffer from rb_screen.c */
extern void *pixel_buffer;
extern screen_specs qlscreen;

/* Key event injection from rb_screen.c */
extern void QLRBSendEvent(RBEvent evt);

/* --- Emulator thread --- */

static pthread_t emu_thread;
static int emu_running = 0;
static int emu_paused = 0;

static void *emu_thread_func(void *arg) {
    (void)arg;
    QLStart();
    emu_running = 0;
    return NULL;
}

/* --- Python module functions --- */

static PyObject *
iql_init(PyObject *self, PyObject *args)
{
    const char *system_path;
    if (!PyArg_ParseTuple(args, "s", &system_path))
        return NULL;

    if (emu_running) {
        QLStop();
        pthread_join(emu_thread, NULL);
        emu_running = 0;
    }

    iql_set_system_path(system_path);

    emu_running = 1;
    if (pthread_create(&emu_thread, NULL, emu_thread_func, NULL) != 0) {
        emu_running = 0;
        PyErr_SetString(PyExc_RuntimeError, "Failed to create emulator thread");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
iql_tick(PyObject *self, PyObject *args)
{
    (void)args;
    if (emu_running && !emu_paused) {
        QLTimer();
    }
    Py_RETURN_NONE;
}

static PyObject *
iql_get_framebuffer(PyObject *self, PyObject *args)
{
    (void)args;
    if (!pixel_buffer || !emu_running) {
        Py_RETURN_NONE;
    }

    int w = qlscreen.xres;
    int h = qlscreen.yres;
    Py_ssize_t size = w * h * 4;

    return PyBytes_FromStringAndSize((const char *)pixel_buffer, size);
}

static PyObject *
iql_get_screen_size(PyObject *self, PyObject *args)
{
    (void)args;
    return Py_BuildValue("(ii)", (int)qlscreen.xres, (int)qlscreen.yres);
}

static PyObject *
iql_send_key(PyObject *self, PyObject *args)
{
    int vk_code;
    int pressed;
    int shift = 0;
    int control = 0;
    int alt = 0;

    if (!PyArg_ParseTuple(args, "ii|iii", &vk_code, &pressed,
                          &shift, &control, &alt))
        return NULL;

    RBEvent event;
    memset(&event, 0, sizeof(event));
    event.type = pressed ? RBEVT_KeyPressed : RBEVT_KeyReleased;
    event.code = (RBVirtualKey)vk_code;
    event.shift = shift;
    event.control = control;
    event.alt = alt;

    QLRBSendEvent(event);

    Py_RETURN_NONE;
}

static PyObject *
iql_restart(PyObject *self, PyObject *args)
{
    (void)args;
    if (emu_running) {
        QLRestart();
    }
    Py_RETURN_NONE;
}

static PyObject *
iql_stop(PyObject *self, PyObject *args)
{
    (void)args;
    if (emu_running) {
        QLStop();
        pthread_join(emu_thread, NULL);
        emu_running = 0;
    }
    Py_RETURN_NONE;
}

static PyObject *
iql_set_speed(PyObject *self, PyObject *args)
{
    int ms;
    if (!PyArg_ParseTuple(args, "i", &ms))
        return NULL;
    QLSetSpeed(ms);
    Py_RETURN_NONE;
}

static PyObject *
iql_is_running(PyObject *self, PyObject *args)
{
    (void)args;
    return PyBool_FromLong(emu_running);
}

static PyObject *
iql_pause(PyObject *self, PyObject *args)
{
    (void)args;
    emu_paused = 1;
    Py_RETURN_NONE;
}

static PyObject *
iql_resume(PyObject *self, PyObject *args)
{
    (void)args;
    emu_paused = 0;
    Py_RETURN_NONE;
}

static PyObject *
iql_is_paused(PyObject *self, PyObject *args)
{
    (void)args;
    return PyBool_FromLong(emu_paused);
}

static PyObject *
iql_get_cpu_state(PyObject *self, PyObject *args)
{
    (void)args;
    if (!emu_running) {
        Py_RETURN_NONE;
    }

    unsigned int pc_addr = (unsigned int)((char *)pc - (char *)theROM);

    return Py_BuildValue(
        "{s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,"
        "s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,"
        "s:I,s:I,s:I,s:H,"
        "s:i,s:i,s:i,s:i}",
        "D0", (unsigned int)reg[0],  "D1", (unsigned int)reg[1],
        "D2", (unsigned int)reg[2],  "D3", (unsigned int)reg[3],
        "D4", (unsigned int)reg[4],  "D5", (unsigned int)reg[5],
        "D6", (unsigned int)reg[6],  "D7", (unsigned int)reg[7],
        "A0", (unsigned int)reg[8],  "A1", (unsigned int)reg[9],
        "A2", (unsigned int)reg[10], "A3", (unsigned int)reg[11],
        "A4", (unsigned int)reg[12], "A5", (unsigned int)reg[13],
        "A6", (unsigned int)reg[14], "A7", (unsigned int)reg[15],
        "PC", pc_addr,
        "USP", (unsigned int)usp,
        "SSP", (unsigned int)ssp,
        "SR", (unsigned short)GetSR(),
        "key_down", gKeyDown,
        "shift", shiftKey,
        "control", controlKey,
        "alt", altKey
    );
}

/* --- Module definition --- */

static PyMethodDef iql_methods[] = {
    {"init",             iql_init,             METH_VARARGS,
     "init(system_path) — Start emulator with given system path"},
    {"tick",             iql_tick,             METH_NOARGS,
     "tick() — Advance emulation by one timer step"},
    {"get_framebuffer",  iql_get_framebuffer,  METH_NOARGS,
     "get_framebuffer() — Return 512x256 RGBA pixel buffer as bytes"},
    {"get_screen_size",  iql_get_screen_size,  METH_NOARGS,
     "get_screen_size() — Return (width, height) tuple"},
    {"send_key",         iql_send_key,         METH_VARARGS,
     "send_key(vk_code, pressed, shift=0, ctrl=0, alt=0) — Send key event"},
    {"restart",          iql_restart,          METH_NOARGS,
     "restart() — Restart the emulator"},
    {"stop",             iql_stop,             METH_NOARGS,
     "stop() — Stop the emulator thread"},
    {"set_speed",        iql_set_speed,        METH_VARARGS,
     "set_speed(ms) — Set CPU throttle (0=normal, 4=slow)"},
    {"is_running",       iql_is_running,       METH_NOARGS,
     "is_running() — Check if emulator thread is alive"},
    {"pause",            iql_pause,            METH_NOARGS,
     "pause() — Pause emulation (tick becomes no-op)"},
    {"resume",           iql_resume,           METH_NOARGS,
     "resume() — Resume emulation after pause"},
    {"is_paused",        iql_is_paused,        METH_NOARGS,
     "is_paused() — Check if emulator is paused"},
    {"get_cpu_state",    iql_get_cpu_state,    METH_NOARGS,
     "get_cpu_state() — Return dict of CPU registers and keyboard state"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef iql_module = {
    PyModuleDef_HEAD_INIT,
    "_iql",
    "iQL Sinclair QL emulator extension",
    -1,
    iql_methods
};

/* Export virtual key constants so Python can reference them */
static void add_vk_constants(PyObject *m) {
    /* Letters */
    PyModule_AddIntConstant(m, "VK_A", RBVK_A);
    PyModule_AddIntConstant(m, "VK_B", RBVK_B);
    PyModule_AddIntConstant(m, "VK_C", RBVK_C);
    PyModule_AddIntConstant(m, "VK_D", RBVK_D);
    PyModule_AddIntConstant(m, "VK_E", RBVK_E);
    PyModule_AddIntConstant(m, "VK_F", RBVK_F);
    PyModule_AddIntConstant(m, "VK_G", RBVK_G);
    PyModule_AddIntConstant(m, "VK_H", RBVK_H);
    PyModule_AddIntConstant(m, "VK_I", RBVK_I);
    PyModule_AddIntConstant(m, "VK_J", RBVK_J);
    PyModule_AddIntConstant(m, "VK_K", RBVK_K);
    PyModule_AddIntConstant(m, "VK_L", RBVK_L);
    PyModule_AddIntConstant(m, "VK_M", RBVK_M);
    PyModule_AddIntConstant(m, "VK_N", RBVK_N);
    PyModule_AddIntConstant(m, "VK_O", RBVK_O);
    PyModule_AddIntConstant(m, "VK_P", RBVK_P);
    PyModule_AddIntConstant(m, "VK_Q", RBVK_Q);
    PyModule_AddIntConstant(m, "VK_R", RBVK_R);
    PyModule_AddIntConstant(m, "VK_S", RBVK_S);
    PyModule_AddIntConstant(m, "VK_T", RBVK_T);
    PyModule_AddIntConstant(m, "VK_U", RBVK_U);
    PyModule_AddIntConstant(m, "VK_V", RBVK_V);
    PyModule_AddIntConstant(m, "VK_W", RBVK_W);
    PyModule_AddIntConstant(m, "VK_X", RBVK_X);
    PyModule_AddIntConstant(m, "VK_Y", RBVK_Y);
    PyModule_AddIntConstant(m, "VK_Z", RBVK_Z);

    /* Numbers */
    PyModule_AddIntConstant(m, "VK_0", RBVK_Num0);
    PyModule_AddIntConstant(m, "VK_1", RBVK_Num1);
    PyModule_AddIntConstant(m, "VK_2", RBVK_Num2);
    PyModule_AddIntConstant(m, "VK_3", RBVK_Num3);
    PyModule_AddIntConstant(m, "VK_4", RBVK_Num4);
    PyModule_AddIntConstant(m, "VK_5", RBVK_Num5);
    PyModule_AddIntConstant(m, "VK_6", RBVK_Num6);
    PyModule_AddIntConstant(m, "VK_7", RBVK_Num7);
    PyModule_AddIntConstant(m, "VK_8", RBVK_Num8);
    PyModule_AddIntConstant(m, "VK_9", RBVK_Num9);

    /* Special keys */
    PyModule_AddIntConstant(m, "VK_SPACE", RBVK_Space);
    PyModule_AddIntConstant(m, "VK_RETURN", RBVK_Return);
    PyModule_AddIntConstant(m, "VK_ESCAPE", RBVK_Escape);
    PyModule_AddIntConstant(m, "VK_TAB", RBVK_Tab);
    PyModule_AddIntConstant(m, "VK_BACKSPACE", RBVK_BackSpace);

    /* Arrow keys */
    PyModule_AddIntConstant(m, "VK_LEFT", RBVK_Left);
    PyModule_AddIntConstant(m, "VK_RIGHT", RBVK_Right);
    PyModule_AddIntConstant(m, "VK_UP", RBVK_Up);
    PyModule_AddIntConstant(m, "VK_DOWN", RBVK_Down);

    /* Function keys */
    PyModule_AddIntConstant(m, "VK_F1", RBVK_F1);
    PyModule_AddIntConstant(m, "VK_F2", RBVK_F2);
    PyModule_AddIntConstant(m, "VK_F3", RBVK_F3);
    PyModule_AddIntConstant(m, "VK_F4", RBVK_F4);
    PyModule_AddIntConstant(m, "VK_F5", RBVK_F5);

    /* Modifiers */
    PyModule_AddIntConstant(m, "VK_LSHIFT", RBVK_LShift);
    PyModule_AddIntConstant(m, "VK_RSHIFT", RBVK_RShift);
    PyModule_AddIntConstant(m, "VK_LCONTROL", RBVK_LControl);
    PyModule_AddIntConstant(m, "VK_RCONTROL", RBVK_RControl);
    PyModule_AddIntConstant(m, "VK_LALT", RBVK_LAlt);
    PyModule_AddIntConstant(m, "VK_RALT", RBVK_RAlt);

    /* Punctuation */
    PyModule_AddIntConstant(m, "VK_COMMA", RBVK_Comma);
    PyModule_AddIntConstant(m, "VK_PERIOD", RBVK_Period);
    PyModule_AddIntConstant(m, "VK_SLASH", RBVK_Slash);
    PyModule_AddIntConstant(m, "VK_SEMICOLON", RBVK_SemiColon);
    PyModule_AddIntConstant(m, "VK_QUOTE", RBVK_Quote);
    PyModule_AddIntConstant(m, "VK_BACKSLASH", RBVK_BackSlash);
    PyModule_AddIntConstant(m, "VK_DASH", RBVK_Dash);
    PyModule_AddIntConstant(m, "VK_EQUAL", RBVK_Equal);
    PyModule_AddIntConstant(m, "VK_GRAVE", RBVK_Grave);
    PyModule_AddIntConstant(m, "VK_LBRACKET", RBVK_LBracket);
    PyModule_AddIntConstant(m, "VK_RBRACKET", RBVK_RBracket);
}

PyMODINIT_FUNC
PyInit__iql(void)
{
    PyObject *m = PyModule_Create(&iql_module);
    if (m == NULL)
        return NULL;

    add_vk_constants(m);

    return m;
}
