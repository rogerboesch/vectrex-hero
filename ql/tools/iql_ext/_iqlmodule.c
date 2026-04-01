/*
 * _iqlmodule.c — CPython extension wrapping the iQL emulator core
 *
 * Exposes the Sinclair QL emulator as a Python module for embedding
 * in QL Studio (qlstudio.py).
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
extern void QLPause(void);
extern void QLResume(void);

/* Our platform stub */
extern void iql_set_system_path(const char *path);
extern int iql_drain_log(char *out, int max_len);

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

/* Forward declarations */
static void check_breakpoints_after_tick(void);

/* Trap logging — our own hook called from patched base_instructions_pz.c */
extern int tracetrap;
extern void rb_log_debug(const char *fmt, ...);

/* Circular trap log buffer */
#define TRAP_LOG_SIZE 4096
static char trap_log_buf[TRAP_LOG_SIZE];
static int trap_log_head = 0;
static int trap_log_tail = 0;
static int trap_logging_enabled = 0;

static void trap_log_append(const char *str) {
    while (*str) {
        trap_log_buf[trap_log_head] = *str++;
        trap_log_head = (trap_log_head + 1) % TRAP_LOG_SIZE;
        if (trap_log_head == trap_log_tail)
            trap_log_tail = (trap_log_tail + 1) % TRAP_LOG_SIZE;
    }
}

/* Trap names for the log */
static const char *trap1_names[] = {
    "MT.INF","MT.CJOB","MT.JINF",NULL,"MT.RJOB","MT.FRJOB",
    "MT.FREE","MT.TRAPV","MT.SUSJB","MT.RELJB","MT.ACTIV",
    "MT.PRIOR","MT.ALLOC","MT.LNKFR","MT.ALRES","MT.RERES",
    "MT.DMODE","MT.IPCOM","MT.BAUD","MT.RCLCK","MT.SCLCK",
    "MT.ACLCK","MT.ALBAS","MT.REBAS","MT.LXINT","MT.RXINT",
    "MT.LPOLL","MT.RPOLL","MT.LSCHD","MT.RSCHD",
    "MT.LIOD","MT.RIOD","MT.LDD","MT.RDD"
};
static const char *trap2_names[] = { "IO.OPEN", "IO.CLOSE" };
static const char *trap3_names[] = {
    "IO.PEND","IO.FBYTE","IO.FLINE","IO.FSTRG",NULL,
    "IO.SBYTE","IO.SSTRG","IO.EDLIN"
};

/* Called from patched TRAP instruction handler */
void iql_trap_hook(int trap_num) {
    if (!trap_logging_enabled) return;

    char buf[128];
    int d0 = (int)reg[0];
    int d1 = (int)reg[1];
    int d3 = (int)reg[3];
    const char *name = NULL;

    if (trap_num == 1) {
        /* Manager trap — d0 = function code */
        int fn = d0 & 0xFF;
        if (fn < (int)(sizeof(trap1_names)/sizeof(trap1_names[0])))
            name = trap1_names[fn];
        snprintf(buf, sizeof(buf), "TRAP #1  d0=$%02X %-12s d1=$%08X d3=$%04X\n",
                 fn, name ? name : "?", (unsigned)d1, d3 & 0xFFFF);
    } else if (trap_num == 2) {
        int fn = d0 & 0xFF;
        if (fn < (int)(sizeof(trap2_names)/sizeof(trap2_names[0])))
            name = trap2_names[fn];
        snprintf(buf, sizeof(buf), "TRAP #2  d0=$%02X %-12s d1=$%08X\n",
                 fn, name ? name : "?", (unsigned)d1);
    } else if (trap_num == 3) {
        int fn = d0 & 0xFF;
        if (fn < (int)(sizeof(trap3_names)/sizeof(trap3_names[0])))
            name = trap3_names[fn];
        snprintf(buf, sizeof(buf), "TRAP #3  d0=$%02X %-12s d3=$%04X\n",
                 fn, name ? name : "?", d3 & 0xFFFF);
    } else {
        snprintf(buf, sizeof(buf), "TRAP #%d  d0=$%08X\n", trap_num, (unsigned)d0);
    }
    trap_log_append(buf);
}

/* Frame step: pause on next TRAP #1 (QDOS manager trap) */
static int frame_step_active = 0;

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
        check_breakpoints_after_tick();
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
    if (!emu_paused) {
        QLPause();
        emu_paused = 1;
    }
    Py_RETURN_NONE;
}

static PyObject *
iql_resume(PyObject *self, PyObject *args)
{
    (void)args;
    if (emu_paused) {
        QLResume();
        emu_paused = 0;
    }
    Py_RETURN_NONE;
}

static PyObject *
iql_is_paused(PyObject *self, PyObject *args)
{
    (void)args;
    return PyBool_FromLong(emu_paused);
}

#define GET_LOG_BUF_SIZE 8192

static PyObject *
iql_get_log(PyObject *self, PyObject *args)
{
    (void)args;
    char buf[GET_LOG_BUF_SIZE];
    int len = iql_drain_log(buf, GET_LOG_BUF_SIZE);
    if (len == 0) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromStringAndSize(buf, len);
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

/* --- Memory access --- */

extern rw8 ReadByte(aw32 addr);
extern rw16 ReadWord(aw32 addr);
extern rw32 ReadLong(aw32 addr);
extern void WriteByte(aw32 addr, aw8 d);
extern void WriteWord(aw32 addr, aw16 d);
extern void WriteLong(aw32 addr, aw32 d);
extern void ExecuteChunk(long n);

static PyObject *
iql_read_byte(PyObject *self, PyObject *args)
{
    unsigned int addr;
    if (!PyArg_ParseTuple(args, "I", &addr))
        return NULL;
    return PyLong_FromLong((unsigned char)ReadByte(addr));
}

static PyObject *
iql_read_word(PyObject *self, PyObject *args)
{
    unsigned int addr;
    if (!PyArg_ParseTuple(args, "I", &addr))
        return NULL;
    return PyLong_FromLong((unsigned short)ReadWord(addr));
}

static PyObject *
iql_read_long(PyObject *self, PyObject *args)
{
    unsigned int addr;
    if (!PyArg_ParseTuple(args, "I", &addr))
        return NULL;
    return PyLong_FromUnsignedLong((unsigned int)ReadLong(addr));
}

static PyObject *
iql_read_mem(PyObject *self, PyObject *args)
{
    unsigned int addr, length;
    if (!PyArg_ParseTuple(args, "II", &addr, &length))
        return NULL;
    if (length > 65536) {
        PyErr_SetString(PyExc_ValueError, "length too large (max 65536)");
        return NULL;
    }
    PyObject *bytes = PyBytes_FromStringAndSize(NULL, length);
    if (!bytes) return NULL;
    char *buf = PyBytes_AS_STRING(bytes);
    unsigned int i;
    for (i = 0; i < length; i++)
        buf[i] = (char)ReadByte(addr + i);
    return bytes;
}

static PyObject *
iql_write_byte(PyObject *self, PyObject *args)
{
    unsigned int addr;
    int value;
    if (!PyArg_ParseTuple(args, "Ii", &addr, &value))
        return NULL;
    WriteByte(addr, value & 0xFF);
    Py_RETURN_NONE;
}

static PyObject *
iql_write_word(PyObject *self, PyObject *args)
{
    unsigned int addr;
    int value;
    if (!PyArg_ParseTuple(args, "Ii", &addr, &value))
        return NULL;
    WriteWord(addr, value & 0xFFFF);
    Py_RETURN_NONE;
}

static PyObject *
iql_write_long(PyObject *self, PyObject *args)
{
    unsigned int addr;
    unsigned int value;
    if (!PyArg_ParseTuple(args, "II", &addr, &value))
        return NULL;
    WriteLong(addr, value);
    Py_RETURN_NONE;
}

/* --- Single step --- */

static PyObject *
iql_step(PyObject *self, PyObject *args)
{
    int count = 1;
    if (!PyArg_ParseTuple(args, "|i", &count))
        return NULL;
    if (count < 1) count = 1;
    if (count > 100000) count = 100000;
    if (emu_running) {
        ExecuteChunk(count);
    }
    Py_RETURN_NONE;
}

/* --- Breakpoints --- */

#define MAX_BREAKPOINTS 16
static unsigned int breakpoints[MAX_BREAKPOINTS];
static int num_breakpoints = 0;
static int breakpoint_hit = 0;   /* set to 1 when BP triggers */
static unsigned int bp_hit_addr = 0;

static PyObject *
iql_add_breakpoint(PyObject *self, PyObject *args)
{
    unsigned int addr;
    if (!PyArg_ParseTuple(args, "I", &addr))
        return NULL;
    if (num_breakpoints >= MAX_BREAKPOINTS) {
        PyErr_SetString(PyExc_RuntimeError, "max breakpoints reached");
        return NULL;
    }
    breakpoints[num_breakpoints++] = addr;
    Py_RETURN_NONE;
}

static PyObject *
iql_remove_breakpoint(PyObject *self, PyObject *args)
{
    unsigned int addr;
    int i, j;
    if (!PyArg_ParseTuple(args, "I", &addr))
        return NULL;
    for (i = 0; i < num_breakpoints; i++) {
        if (breakpoints[i] == addr) {
            for (j = i; j < num_breakpoints - 1; j++)
                breakpoints[j] = breakpoints[j + 1];
            num_breakpoints--;
            Py_RETURN_TRUE;
        }
    }
    Py_RETURN_FALSE;
}

static PyObject *
iql_clear_breakpoints(PyObject *self, PyObject *args)
{
    (void)args;
    num_breakpoints = 0;
    breakpoint_hit = 0;
    Py_RETURN_NONE;
}

static PyObject *
iql_list_breakpoints(PyObject *self, PyObject *args)
{
    (void)args;
    PyObject *list = PyList_New(num_breakpoints);
    int i;
    for (i = 0; i < num_breakpoints; i++)
        PyList_SET_ITEM(list, i, PyLong_FromUnsignedLong(breakpoints[i]));
    return list;
}

static PyObject *
iql_check_breakpoint(PyObject *self, PyObject *args)
{
    (void)args;
    if (breakpoint_hit) {
        breakpoint_hit = 0;
        return PyLong_FromUnsignedLong(bp_hit_addr);
    }
    Py_RETURN_NONE;
}

/* Call this from the tick function to check breakpoints */
static void check_breakpoints_after_tick(void)
{
    if (num_breakpoints > 0 && emu_running && !emu_paused) {
        unsigned int pc_addr = (unsigned int)((char *)pc - (char *)theROM);
        int i;
        for (i = 0; i < num_breakpoints; i++) {
            if (breakpoints[i] == pc_addr) {
                QLPause();
                emu_paused = 1;
                breakpoint_hit = 1;
                bp_hit_addr = pc_addr;
                break;
            }
        }
    }
}

/* --- Screenshot --- */

static PyObject *
iql_screenshot(PyObject *self, PyObject *args)
{
    const char *filename;
    if (!PyArg_ParseTuple(args, "s", &filename))
        return NULL;

    if (!pixel_buffer || !emu_running) {
        PyErr_SetString(PyExc_RuntimeError, "emulator not running");
        return NULL;
    }

    /* Return raw RGBA data + dimensions — Python side saves as PNG */
    int w = qlscreen.xres;
    int h = qlscreen.yres;
    Py_ssize_t size = w * h * 4;

    PyObject *result = Py_BuildValue("{s:y#,s:i,s:i,s:s}",
        "data", (const char *)pixel_buffer, size,
        "width", w,
        "height", h,
        "filename", filename);
    return result;
}

/* --- Trap logging --- */

static PyObject *
iql_set_trap_logging(PyObject *self, PyObject *args)
{
    int enable;
    if (!PyArg_ParseTuple(args, "i", &enable))
        return NULL;
    trap_logging_enabled = enable ? 1 : 0;
    Py_RETURN_NONE;
}

static PyObject *
iql_get_trap_logging(PyObject *self, PyObject *args)
{
    (void)args;
    return PyBool_FromLong(trap_logging_enabled);
}

/* Drain trap log buffer */
static PyObject *
iql_get_trap_log(PyObject *self, PyObject *args)
{
    (void)args;
    if (trap_log_head == trap_log_tail)
        Py_RETURN_NONE;

    char out[TRAP_LOG_SIZE];
    int len = 0;
    while (trap_log_tail != trap_log_head && len < TRAP_LOG_SIZE - 1) {
        out[len++] = trap_log_buf[trap_log_tail];
        trap_log_tail = (trap_log_tail + 1) % TRAP_LOG_SIZE;
    }
    return PyUnicode_FromStringAndSize(out, len);
}

/* --- Frame step --- */

/*
 * iql_step_frame: call QLTimer() directly to advance one emulator step
 * (CPU execution + screen refresh). Each call ≈ 3000 instructions + display update.
 * Call multiple times for a full game frame.
 */
/* Screen render function from iQL — converts QL screen memory to RGBA pixel buffer */
extern void ql_render_screen(void *buffer);

static PyObject *
iql_step_frame(PyObject *self, PyObject *args)
{
    int count = 1;
    if (!PyArg_ParseTuple(args, "|i", &count))
        return NULL;
    if (count < 1) count = 1;
    if (count > 200) count = 200;

    if (!emu_running) Py_RETURN_NONE;

    /* Execute instructions directly (works even when paused) */
    {
        int i;
        for (i = 0; i < count; i++)
            ExecuteChunk(3000);
    }

    /* Force screen buffer refresh so display updates */
    if (pixel_buffer)
        ql_render_screen(pixel_buffer);

    Py_RETURN_NONE;
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
    {"get_log",          iql_get_log,          METH_NOARGS,
     "get_log() — Drain log buffer, return str or None"},

    /* Memory access */
    {"read_byte",        iql_read_byte,        METH_VARARGS,
     "read_byte(addr) — Read byte from QL memory"},
    {"read_word",        iql_read_word,        METH_VARARGS,
     "read_word(addr) — Read 16-bit word from QL memory"},
    {"read_long",        iql_read_long,        METH_VARARGS,
     "read_long(addr) — Read 32-bit long from QL memory"},
    {"read_mem",         iql_read_mem,         METH_VARARGS,
     "read_mem(addr, length) — Read block of QL memory as bytes"},
    {"write_byte",       iql_write_byte,       METH_VARARGS,
     "write_byte(addr, value) — Write byte to QL memory"},
    {"write_word",       iql_write_word,       METH_VARARGS,
     "write_word(addr, value) — Write 16-bit word to QL memory"},
    {"write_long",       iql_write_long,       METH_VARARGS,
     "write_long(addr, value) — Write 32-bit long to QL memory"},

    /* Single step */
    {"step",             iql_step,             METH_VARARGS,
     "step(count=1) — Execute count 68K instructions (paused mode)"},

    /* Breakpoints */
    {"add_breakpoint",    iql_add_breakpoint,    METH_VARARGS,
     "add_breakpoint(addr) — Set breakpoint at address"},
    {"remove_breakpoint", iql_remove_breakpoint, METH_VARARGS,
     "remove_breakpoint(addr) — Remove breakpoint, returns True if found"},
    {"clear_breakpoints", iql_clear_breakpoints, METH_NOARGS,
     "clear_breakpoints() — Remove all breakpoints"},
    {"list_breakpoints",  iql_list_breakpoints,  METH_NOARGS,
     "list_breakpoints() — Return list of breakpoint addresses"},
    {"check_breakpoint",  iql_check_breakpoint,  METH_NOARGS,
     "check_breakpoint() — Return addr if BP hit since last check, else None"},

    /* Screenshot */
    {"screenshot",       iql_screenshot,       METH_VARARGS,
     "screenshot(filename) — Return RGBA data dict for saving as PNG"},

    /* Trap logging */
    {"set_trap_logging", iql_set_trap_logging, METH_VARARGS,
     "set_trap_logging(enable) — Enable/disable QDOS trap logging to console"},
    {"get_trap_logging", iql_get_trap_logging, METH_NOARGS,
     "get_trap_logging() — Check if trap logging is enabled"},
    {"get_trap_log",     iql_get_trap_log,     METH_NOARGS,
     "get_trap_log() — Drain trap log buffer, return str or None"},

    /* Frame step */
    {"step_frame",       iql_step_frame,       METH_VARARGS,
     "step_frame(count=1) — Run count QLTimer steps (each ≈ 3K instr + screen update)"},

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
