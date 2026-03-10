#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include "vec2x.h"
#include "e6809.h"

/* ---- frame callback state ---- */

/* Max vectors per frame = VECTREX_MHZ / phosphor_decay_rate */
#define FRAME_VECTOR_MAX (VECTREX_MHZ / 30)

static int frame_ready = 0;
static vector_t frame_copy[FRAME_VECTOR_MAX];
static long frame_vector_count = 0;

static void py_frame_callback(void)
{
    frame_ready = 1;
    frame_vector_count = vector_draw_cnt;
    /* Snapshot the vector data now, before the frame swap in vec2x_emu()
     * invalidates entries via alg_addline deduplication. */
    if (frame_vector_count > 0)
        memcpy(frame_copy, vectors_draw, frame_vector_count * sizeof(vector_t));
}

/* ---- module functions ---- */

static PyObject *
vec2x_init(PyObject *self, PyObject *args)
{
    Py_buffer rom_buf, cart_buf;

    if (!PyArg_ParseTuple(args, "y*y*", &rom_buf, &cart_buf))
        return NULL;

    /* Copy ROM data */
    Py_ssize_t rom_len = rom_buf.len;
    if (rom_len > 8192) rom_len = 8192;
    memset(rom, 0, 8192);
    memcpy(rom, rom_buf.buf, rom_len);

    /* Copy cart data */
    Py_ssize_t cart_len = cart_buf.len;
    if (cart_len > 32768) cart_len = 32768;
    memset(cart, 0, 32768);
    memcpy(cart, cart_buf.buf, cart_len);

    PyBuffer_Release(&rom_buf);
    PyBuffer_Release(&cart_buf);

    /* Install frame callback */
    vec2x_frame_callback = py_frame_callback;
    frame_ready = 0;

    Py_RETURN_NONE;
}

static PyObject *
vec2x_reset_py(PyObject *self, PyObject *args)
{
    vec2x_reset();
    frame_ready = 0;
    Py_RETURN_NONE;
}

static PyObject *
vec2x_emu_tick(PyObject *self, PyObject *args)
{
    long cycles;

    if (!PyArg_ParseTuple(args, "l", &cycles))
        return NULL;

    frame_ready = 0;

    vec2x_emu(cycles);

    if (!frame_ready) {
        Py_RETURN_NONE;
    }

    /* Build a list of (x0, y0, x1, y1, color) tuples */
    PyObject *list = PyList_New(frame_vector_count);
    if (!list)
        return NULL;

    for (long i = 0; i < frame_vector_count; i++) {
        vector_t *v = &frame_copy[i];
        PyObject *tup = Py_BuildValue("(llllB)",
            v->x0, v->y0, v->x1, v->y1, v->color);
        if (!tup) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, i, tup);  /* steals ref */
    }

    return list;
}

static PyObject *
vec2x_set_input(PyObject *self, PyObject *args)
{
    unsigned buttons, jx, jy;

    if (!PyArg_ParseTuple(args, "III", &buttons, &jx, &jy))
        return NULL;

    snd_regs[14] = buttons;
    alg_jch0 = jx;
    alg_jch1 = jy;

    Py_RETURN_NONE;
}

static PyObject *
vec2x_get_state(PyObject *self, PyObject *args)
{
    return Py_BuildValue("{s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:l}",
        "A",  reg_a,
        "B",  reg_b,
        "X",  reg_x,
        "Y",  reg_y,
        "U",  reg_u,
        "S",  reg_s,
        "PC", reg_pc,
        "DP", reg_dp,
        "CC", reg_cc,
        "vectors", frame_vector_count);
}

/* ---- module definition ---- */

static PyMethodDef vec2x_methods[] = {
    {"init",      vec2x_init,      METH_VARARGS, "init(rom_bytes, cart_bytes) -> None"},
    {"reset",     vec2x_reset_py,  METH_NOARGS,  "reset() -> None"},
    {"emu_tick",  vec2x_emu_tick,  METH_VARARGS, "emu_tick(cycles) -> list or None"},
    {"set_input", vec2x_set_input, METH_VARARGS, "set_input(buttons, jx, jy) -> None"},
    {"get_state", vec2x_get_state, METH_NOARGS,  "get_state() -> dict"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef vec2x_module = {
    PyModuleDef_HEAD_INIT,
    "_vec2x",
    "C extension wrapper for vec2x Vectrex emulator",
    -1,
    vec2x_methods
};

PyMODINIT_FUNC
PyInit__vec2x(void)
{
    return PyModule_Create(&vec2x_module);
}
