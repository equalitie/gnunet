/*
   This file is part of GNUnet.
   Copyright (C) 2012-2015 GNUnet e.V.

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
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
   */
/**
 * @author Martin Schanzenbach
 * @file src/identity-provider/gnunet-idp.c
 * @brief Identity Provider utility
 *
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_namestore_service.h"
#include "gnunet_identity_provider_service.h"
#include "gnunet_identity_service.h"
#include "gnunet_signatures.h"

/**
 * List attribute flag
 */
static int list;

/**
 * Relying party
 */
static char* rp;

/**
 * The attribute
 */
static char* attr_name;

/**
 * Attribute value
 */
static char* attr_value;

/**
 * Attributes to issue
 */
static char* issue_attrs;

/**
 * Ticket to consume
 */
static char* consume_ticket;

/**
 * Ego name
 */
static char* ego_name;

/**
 * Identity handle
 */
static struct GNUNET_IDENTITY_Handle *identity_handle;

/**
 * IdP handle
 */
static struct GNUNET_IDENTITY_PROVIDER_Handle *idp_handle;

/**
 * IdP operation
 */
static struct GNUNET_IDENTITY_PROVIDER_Operation *idp_op;

/**
 * Attribute iterator
 */
static struct GNUNET_IDENTITY_PROVIDER_AttributeIterator *attr_iterator;

/**
 * Master ABE key
 */
static struct GNUNET_CRYPTO_AbeMasterKey *abe_key;

/**
 * ego private key
 */
static const struct GNUNET_CRYPTO_EcdsaPrivateKey *pkey;

/**
 * rp public key
 */
static struct GNUNET_CRYPTO_EcdsaPublicKey rp_key;

/**
 * Ticket to consume
 */
static struct GNUNET_IDENTITY_PROVIDER_Ticket ticket;

/**
 * Attribute list
 */
static struct GNUNET_IDENTITY_PROVIDER_AttributeList *attr_list;

static void
do_cleanup(void *cls)
{
  if (NULL != attr_iterator)
    GNUNET_IDENTITY_PROVIDER_get_attributes_stop (attr_iterator);
  if (NULL != idp_handle)
    GNUNET_IDENTITY_PROVIDER_disconnect (idp_handle);
  if (NULL != identity_handle)
    GNUNET_IDENTITY_disconnect (identity_handle);
  if (NULL != abe_key)
    GNUNET_free (abe_key);
  if (NULL != attr_list)
    GNUNET_free (attr_list);
}

static void
ticket_issue_cb (void* cls,
                 const struct GNUNET_IDENTITY_PROVIDER_Ticket *ticket)
{
  char* ticket_str;
  if (NULL != ticket) {
    ticket_str = GNUNET_STRINGS_data_to_string_alloc (ticket,
                                                      sizeof (struct GNUNET_IDENTITY_PROVIDER_Ticket));
    printf("%s\n",
           ticket_str);
    GNUNET_free (ticket_str);
  }
  GNUNET_SCHEDULER_add_now (&do_cleanup, NULL);
}

static void
store_attr_cont (void *cls,
                 int32_t success,
                 const char*emsg)
{
  if (GNUNET_SYSERR == success) {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "%s\n", emsg);
  } else {
    GNUNET_log (GNUNET_ERROR_TYPE_MESSAGE,
                "Successfully added identity attribute %s=%s\n",
                attr_name, attr_value);
  }
  GNUNET_SCHEDULER_add_now (&do_cleanup, NULL);
}

static void
process_attrs (void *cls,
         const struct GNUNET_CRYPTO_EcdsaPublicKey *identity,
         const struct GNUNET_IDENTITY_PROVIDER_Attribute *attr)
{
  if (NULL == identity)
  {
    GNUNET_SCHEDULER_add_now (&do_cleanup, NULL);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_MESSAGE,
              "%s: %s\n", attr->name, (char*)attr->data);
}


static void
iter_error (void *cls)
{
  attr_iterator = NULL;
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
              "Failed to iterate over attributes\n");
  GNUNET_SCHEDULER_add_now (&do_cleanup, NULL);
}

static void
iter_finished (void *cls)
{
  struct GNUNET_IDENTITY_PROVIDER_Attribute *attr;

  attr_iterator = NULL;
  if (list) {
    GNUNET_SCHEDULER_add_now (&do_cleanup, NULL);
    return;
  }

  if (issue_attrs) {
    idp_op = GNUNET_IDENTITY_PROVIDER_ticket_issue (idp_handle,
                                                    pkey,
                                                    &rp_key,
                                                    attr_list,
                                                    &ticket_issue_cb,
                                                    NULL);
    return;
  }
  if (consume_ticket) {
    idp_op = GNUNET_IDENTITY_PROVIDER_ticket_consume (idp_handle,
                                                      pkey,
                                                      &ticket,
                                                      &process_attrs,
                                                      NULL);
    return;
  }
  attr = GNUNET_IDENTITY_PROVIDER_attribute_new (attr_name,
                                                 GNUNET_IDENTITY_PROVIDER_AT_STRING,
                                                 attr_value,
                                                 strlen (attr_value));
  idp_op = GNUNET_IDENTITY_PROVIDER_attribute_store (idp_handle,
                                                     pkey,
                                                     attr,
                                                     &store_attr_cont,
                                                     NULL);


}

static void
iter_cb (void *cls,
         const struct GNUNET_CRYPTO_EcdsaPublicKey *identity,
         const struct GNUNET_IDENTITY_PROVIDER_Attribute *attr)
{
  struct GNUNET_IDENTITY_PROVIDER_AttributeListEntry *le;
  char *attrs_tmp;
  char *attr_str;

  if (issue_attrs)
  {
    attrs_tmp = GNUNET_strdup (issue_attrs);
    attr_str = strtok (attrs_tmp, ",");
    while (NULL != attr_str) {
      if (0 != strcmp (attr_str, attr->name)) {
        attr_str = strtok (NULL, ",");
        continue;
      }
      le = GNUNET_new (struct GNUNET_IDENTITY_PROVIDER_AttributeListEntry);
      le->attribute = GNUNET_IDENTITY_PROVIDER_attribute_new (attr->name,
                                                              attr->attribute_type,
                                                              attr->data,
                                                              attr->data_size);
      GNUNET_CONTAINER_DLL_insert (attr_list->list_head,
                                   attr_list->list_tail,
                                   le);
      break;
    }
    GNUNET_free (attrs_tmp);
  } else if (list) {
    GNUNET_log (GNUNET_ERROR_TYPE_MESSAGE,
                "%s: %s\n", attr->name, (char*)attr->data);
  }
  GNUNET_IDENTITY_PROVIDER_get_attributes_next (attr_iterator);
}

static void
ego_cb (void *cls,
        struct GNUNET_IDENTITY_Ego *ego,
        void **ctx,
        const char *name)
{
  if (NULL == name)
    return;
  if (0 != strcmp (name, ego_name))
    return;
  pkey = GNUNET_IDENTITY_ego_get_private_key (ego);

  if (NULL != rp)
    GNUNET_CRYPTO_ecdsa_public_key_from_string (rp,
                                                strlen (rp),
                                                &rp_key);
  if (NULL != consume_ticket)
    GNUNET_STRINGS_string_to_data (consume_ticket,
                                   strlen (consume_ticket),
                                   &ticket,
                                   sizeof (struct GNUNET_IDENTITY_PROVIDER_Ticket));

  attr_list = GNUNET_new (struct GNUNET_IDENTITY_PROVIDER_AttributeList);

  attr_iterator = GNUNET_IDENTITY_PROVIDER_get_attributes_start (idp_handle,
                                                                 pkey,
                                                                 &iter_error,
                                                                 NULL,
                                                                 &iter_cb,
                                                                 NULL,
                                                                 &iter_finished,
                                                                 NULL);


}

static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *c)
{

  if (NULL == ego_name)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_MESSAGE,
                _("Ego is required\n"));
    return;
  } 

  idp_handle = GNUNET_IDENTITY_PROVIDER_connect (c);
  //Get Ego
  identity_handle = GNUNET_IDENTITY_connect (c,
                                             &ego_cb,
                                             NULL);


}


int
main(int argc, char *const argv[])
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {

    GNUNET_GETOPT_option_string ('a',
                                 "add",
                                 NULL,
                                 gettext_noop ("Add attribute"),
                                 &attr_name),

    GNUNET_GETOPT_option_string ('V',
                                 "value",
                                 NULL,
                                 gettext_noop ("Attribute value"),
                                 &attr_value),
    GNUNET_GETOPT_option_string ('e',
                                 "ego",
                                 NULL,
                                 gettext_noop ("Ego"),
                                 &ego_name),
    GNUNET_GETOPT_option_string ('r',
                                 "rp",
                                 NULL,
                                 gettext_noop ("Audience (relying party)"),
                                 &rp),
    GNUNET_GETOPT_option_flag ('D',
                               "dump",
                               gettext_noop ("List attributes for Ego"),
                               &list),
    GNUNET_GETOPT_option_string ('i',
                                 "issue",
                                 NULL,
                                 gettext_noop ("Issue a ticket"),
                                 &issue_attrs),
    GNUNET_GETOPT_option_string ('C',
                                 "consume",
                                 NULL,
                                 gettext_noop ("Consume a ticket"),
                                 &consume_ticket),
    GNUNET_GETOPT_OPTION_END
  };
  return GNUNET_PROGRAM_run (argc, argv, "ct",
                             "ct", options,
                             &run, NULL);
}
