/*
 * File: z-quark.c
 * Purpose: Save memory by storing strings in a global array, ensuring
 * that each is only allocated once.
 *
 * Copyright (c) 1997 Ben Harrison
 * Copyright (c) 2007 "Elly"
 * Copyright (c) 2025 MAngband and PWMAngband Developers
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "s-angband.h"


static char **quarks;
static size_t nr_quarks = 1;
static size_t alloc_quarks = 0;


#define QUARKS_INIT 16


quark_t quark_add(const char *str)
{
    quark_t q;

    for (q = 1; q < nr_quarks; q++)
    {
        if (streq(quarks[q], str)) return q;
    }

    if (nr_quarks == alloc_quarks)
    {
        alloc_quarks *= 2;
        quarks = mem_realloc(quarks, alloc_quarks * sizeof(char *));
    }
    
    q = nr_quarks++;
    quarks[q] = string_make(str);

    return q;
}


const char *quark_str(quark_t q)
{
    return ((q >= nr_quarks)? NULL: quarks[q]);
}


static void quarks_init(void)
{
    alloc_quarks = QUARKS_INIT;
    quarks = mem_zalloc(alloc_quarks * sizeof(char*));
}


static void quarks_free(void)
{
    size_t i;

    for (i = 1; i < nr_quarks; i++) string_free(quarks[i]);

    mem_free(quarks);
    quarks = NULL;
}


struct init_module z_quark_module =
{
    "z-quark",
    quarks_init,
    quarks_free
};
