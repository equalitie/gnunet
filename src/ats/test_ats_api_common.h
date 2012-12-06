/*
     This file is part of GNUnet.
     (C) 2010,2011 Christian Grothoff (and other contributing authors)

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
 * @file ats/test_ats_api_common.h
 * @brief shared definitions
 * @author Christian Grothoff
 * @author Matthias Wachs
 */

#define TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 30)

#define PEERID "2AK99KD8RM9UA9LC3QKA0IQ5UBFC0FBB50EBGCFQT8448DGGACNAC4CJQDD1CPFS494O41U88DJD1FLIG8VA5CQR9IN4L96GP104MVO"

struct Test_Address
{
  char *plugin;
  size_t plugin_len;

  void *addr;
  size_t addr_len;

  struct GNUNET_ATS_Information *ats;
  int ats_count;

  void *session;
};

struct PeerContext
{
  struct GNUNET_PeerIdentity id;

  struct Test_Address *addr;
};

/* end of file test_ats_api_common.h */
