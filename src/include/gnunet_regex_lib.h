/*
     This file is part of GNUnet
     (C) 2012 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/
/**
 * @file include/gnunet_regex_lib.h
 * @brief library to parse regular expressions into dfa
 * @author Maximilian Szengel
 *
 */

#ifndef GNUNET_REGEX_LIB_H
#define GNUNET_REGEX_LIB_H

#include "gnunet_util_lib.h"

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif

/**
 * NFA representation
 */
struct GNUNET_REGEX_Nfa;

/**
 * Construct an NFA data structure by parsing the regex string of 
 * length len.
 *
 * @param regex regular expression string 
 * @param len length of the string
 *
 * @return NFA data structure. Needs to be freed using 
 *         GNUNET_REGEX_destroy_nfa
 */
struct GNUNET_REGEX_Nfa *
GNUNET_REGEX_construct_nfa(const char *regex, size_t len);

/**
 * Free the memory allocated by constructing the GNUNET_REGEX_Nfa
 * data structure.
 *
 * @param n NFA to be destroyed
 */
void
GNUNET_REGEX_destroy_nfa(struct GNUNET_REGEX_Nfa *n);

/**
 * Save the given NFA as a GraphViz dot file
 *
 * @param n NFA to be saved
 * @param filename where to save the file
 */
void
GNUNET_REGEX_save_nfa_graph(struct GNUNET_REGEX_Nfa *n,
                            const char *filename);

#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

/* end of gnunet_regex_lib.h */
#endif
