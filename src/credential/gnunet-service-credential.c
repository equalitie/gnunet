/*
     This file is part of GNUnet.
     Copyright (C) 2011-2013 GNUnet e.V.

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
 * @file credential/gnunet-service-credential.c
 * @brief GNUnet Credential Service (main service)
 * @author Martin Schanzenbach
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_credential_service.h"
#include "gnunet_statistics_service.h"
#include "credential.h"
#include "credential_serialization.h"
#include "gnunet_protocols.h"
#include "gnunet_signatures.h"

#include <gnunet_dnsparser_lib.h>
#include <gnunet_identity_service.h>
#include <gnunet_gnsrecord_lib.h>
#include <gnunet_namestore_service.h>
#include <gnunet_gns_service.h>


#define GNUNET_CREDENTIAL_MAX_LENGTH 255

struct VerifyRequestHandle;

struct DelegationSetQueueEntry;


struct DelegationChainEntry
{
  /**
   * DLL
   */
  struct DelegationChainEntry *next;

  /**
   * DLL
   */
  struct DelegationChainEntry *prev;

  /**
   * The issuer
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey issuer_key;
  
  /**
   * The subject
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey subject_key;
  
  /**
   * The issued attribute
   */
  char *issuer_attribute;
  
  /**
   * The delegated attribute
   */
  char *subject_attribute;
};

/**
 * DLL for record
 */
struct CredentialRecordEntry
{
  /**
   * DLL
   */
  struct CredentialRecordEntry *next;

  /**
   * DLL
   */
  struct CredentialRecordEntry *prev;
  
  /**
   * Number of references in delegation chains
   */
  uint32_t refcount;

  /**
   * Payload
   */
  struct GNUNET_CREDENTIAL_Credential *credential;
};

/**
 * DLL used for delegations
 * Used for OR delegations
 */
struct DelegationQueueEntry
{
  /**
   * DLL
   */
  struct DelegationQueueEntry *next;

  /**
   * DLL
   */
  struct DelegationQueueEntry *prev;

  /**
   * Sets under this Queue
   */
  struct DelegationSetQueueEntry *set_entries_head;

  /**
   * Sets under this Queue
   */
  struct DelegationSetQueueEntry *set_entries_tail;

  /**
   * Parent set
   */
  struct DelegationSetQueueEntry *parent_set;

  /**
   * Required solutions
   */
  uint32_t required_solutions;
};

/**
 * DLL for delegation sets
 * Used for AND delegation set
 */
struct DelegationSetQueueEntry
{
  /**
   * DLL
   */
  struct DelegationSetQueueEntry *next;

  /**
   * DLL
   */
  struct DelegationSetQueueEntry *prev;

    /**
   * GNS handle
   */
  struct GNUNET_GNS_LookupRequest *lookup_request;

  /**
   * Verify handle
   */
  struct VerifyRequestHandle *handle;

  /**
   * Parent attribute delegation
   */
  struct DelegationQueueEntry *parent;

  /**
   * Issuer key
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey *issuer_key;

  /**
   * Queue entries of this set
   */
  struct DelegationQueueEntry *queue_entries_head;

  /**
   * Queue entries of this set
   */
  struct DelegationQueueEntry *queue_entries_tail;

  /**
   * Parent QueueEntry
   */
  struct DelegationQueueEntry *parent_queue_entry;

  /**
   * Issuer attribute delegated to
   */
  char *issuer_attribute;

  /**
   * The current attribute to look up
   */
  char *lookup_attribute;

  /**
   * Trailing attribute context
   */
  char *attr_trailer;

  /**
   * Still to resolve delegation as string
   */
  char *unresolved_attribute_delegation;

  /**
   * The delegation chain entry
   */
  struct DelegationChainEntry *delegation_chain_entry;

};


/**
 * Handle to a lookup operation from api
 */
struct VerifyRequestHandle
{

  /**
   * We keep these in a DLL.
   */
  struct VerifyRequestHandle *next;

  /**
   * We keep these in a DLL.
   */
  struct VerifyRequestHandle *prev;

  /**
   * Handle to the requesting client
   */
  struct GNUNET_SERVICE_Client *client;

  /**
   * GNS handle
   */
  struct GNUNET_GNS_LookupRequest *lookup_request;

  /**
   * Size of delegation tree
   */
  uint32_t delegation_chain_size;

  /**
   * Children of this attribute
   */
  struct DelegationChainEntry *delegation_chain_head;

  /**
   * Children of this attribute
   */
  struct DelegationChainEntry *delegation_chain_tail;

  /**
   * Issuer public key
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey issuer_key;

  /**
   * Issuer attribute
   */
  char *issuer_attribute;

  /**
   * Subject public key
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey subject_key;

  /**
   * Credential DLL
   */
  struct CredentialRecordEntry *cred_chain_head;

  /**
   * Credential DLL
   */
  struct CredentialRecordEntry *cred_chain_tail;

  /**
   * Credential DLL size
   */
  uint32_t cred_chain_size;

  /**
   * Root Delegation Set
   */
  struct DelegationSetQueueEntry *root_set;

  /**
   * Current Delegation Pointer
   */
  struct DelegationQueueEntry *current_delegation;

  /**
   * request id
   */
  uint32_t request_id;

  /**
   * Pending lookups
   */
  uint64_t pending_lookups;

  /**
   * Credential iterator
   */
  struct GNUNET_NAMESTORE_ZoneIterator *cred_collection_iter;

  /**
   * Collect task
   */
  struct GNUNET_SCHEDULER_Task *collect_next_task;

};


/**
 * Head of the DLL.
 */
static struct VerifyRequestHandle *vrh_head;

/**
 * Tail of the DLL.
 */
static struct VerifyRequestHandle *vrh_tail;

/**
 * Handle to the statistics service
 */
static struct GNUNET_STATISTICS_Handle *statistics;

/**
 * Handle to GNS service.
 */
static struct GNUNET_GNS_Handle *gns;


/**
 * Handle to namestore service
 */
static struct GNUNET_NAMESTORE_Handle *namestore;

static void
cleanup_delegation_set (struct DelegationSetQueueEntry *ds_entry)
{
  struct DelegationQueueEntry *dq_entry;
  struct DelegationSetQueueEntry *child;

  if (NULL == ds_entry)
    return;

  for (dq_entry = ds_entry->queue_entries_head;
       NULL != dq_entry;
       dq_entry = ds_entry->queue_entries_head)
  {
    GNUNET_CONTAINER_DLL_remove (ds_entry->queue_entries_head,
                                 ds_entry->queue_entries_tail,
                                 dq_entry);
    for (child = dq_entry->set_entries_head;
         NULL != child;
         child = dq_entry->set_entries_head)
    {
      GNUNET_CONTAINER_DLL_remove (dq_entry->set_entries_head,
                                   dq_entry->set_entries_tail,
                                   child);
      cleanup_delegation_set (child);
    }
    GNUNET_free (dq_entry);
  }
  GNUNET_free_non_null (ds_entry->issuer_key);
  GNUNET_free_non_null (ds_entry->lookup_attribute);
  GNUNET_free_non_null (ds_entry->issuer_attribute);
  GNUNET_free_non_null (ds_entry->unresolved_attribute_delegation);
  GNUNET_free_non_null (ds_entry->attr_trailer);
  if (NULL != ds_entry->lookup_request)
  {
    GNUNET_GNS_lookup_cancel (ds_entry->lookup_request);
    ds_entry->lookup_request = NULL;
  }
  if (NULL != ds_entry->delegation_chain_entry)
  {
    GNUNET_free_non_null (ds_entry->delegation_chain_entry->subject_attribute);
    GNUNET_free_non_null (ds_entry->delegation_chain_entry->issuer_attribute);
    GNUNET_free (ds_entry->delegation_chain_entry);
  }
  GNUNET_free (ds_entry);
}

static void
cleanup_handle (struct VerifyRequestHandle *vrh)
{
  struct CredentialRecordEntry *cr_entry;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Cleaning up...\n");
  if (NULL != vrh->lookup_request)
  {
    GNUNET_GNS_lookup_cancel (vrh->lookup_request);
    vrh->lookup_request = NULL;
  }
  cleanup_delegation_set (vrh->root_set);
  GNUNET_free_non_null (vrh->issuer_attribute);
  for (cr_entry = vrh->cred_chain_head; 
       NULL != vrh->cred_chain_head;
       cr_entry = vrh->cred_chain_head)
  {
    GNUNET_CONTAINER_DLL_remove (vrh->cred_chain_head,
                                 vrh->cred_chain_tail,
                                 cr_entry);
    GNUNET_free_non_null (cr_entry->credential);
    GNUNET_free (cr_entry);
  }
  GNUNET_free (vrh);
}

static void
shutdown_task (void *cls)
{
  struct VerifyRequestHandle *vrh;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Shutting down!\n");

  while (NULL != (vrh = vrh_head))
  {
    //CREDENTIAL_resolver_lookup_cancel (clh->lookup);
    GNUNET_CONTAINER_DLL_remove (vrh_head,
                                 vrh_tail,
                                 vrh);
    cleanup_handle (vrh);
  }

  if (NULL != gns)
  {
    GNUNET_GNS_disconnect (gns);
    gns = NULL;
  }
  if (NULL != namestore)
  {
    GNUNET_NAMESTORE_disconnect (namestore);
    namestore = NULL;
  }
  if (NULL != statistics)
  {
    GNUNET_STATISTICS_destroy (statistics,
                               GNUNET_NO);
    statistics = NULL;
  }

}



static void
send_lookup_response (struct VerifyRequestHandle *vrh)
{
  struct GNUNET_MQ_Envelope *env;
  struct DelegationChainResultMessage *rmsg;
  struct DelegationChainEntry *dce;
  struct GNUNET_CREDENTIAL_Delegation dd[vrh->delegation_chain_size];
  struct GNUNET_CREDENTIAL_Credential cred[vrh->cred_chain_size];
  struct CredentialRecordEntry *cd;
  struct CredentialRecordEntry *tmp;
  size_t size;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Sending response\n");
  dce = vrh->delegation_chain_head;
  for (uint32_t i=0;i<vrh->delegation_chain_size;i++)
  {
    dd[i].issuer_key = dce->issuer_key;
    dd[i].subject_key = dce->subject_key;
    dd[i].issuer_attribute = dce->issuer_attribute;
    dd[i].issuer_attribute_len = strlen (dce->issuer_attribute)+1;
    dd[i].subject_attribute_len = 0;
    dd[i].subject_attribute = NULL;
    if (NULL != dce->subject_attribute)
    {
      dd[i].subject_attribute = dce->subject_attribute;
      dd[i].subject_attribute_len = strlen(dce->subject_attribute)+1;
    }
    dce = dce->next;
  }

  /**
   * Remove all credentials not needed
   */
  for (cd = vrh->cred_chain_head; NULL != cd;)
  {
    if (cd->refcount > 0)
    {
      cd = cd->next;
      continue;
    }
    tmp = cd;
    cd = cd->next;
    GNUNET_CONTAINER_DLL_remove (vrh->cred_chain_head,
                                 vrh->cred_chain_tail,
                                 tmp);
    GNUNET_free (tmp->credential);
    GNUNET_free (tmp);
    vrh->cred_chain_size--;
  }

  /**
   * Get serialized record data
   * Append at the end of rmsg
   */
  cd = vrh->cred_chain_head;
  for (uint32_t i=0;i<vrh->cred_chain_size;i++)
  {
    cred[i].issuer_key = cd->credential->issuer_key;
    cred[i].subject_key = cd->credential->subject_key;
    cred[i].issuer_attribute_len = strlen(cd->credential->issuer_attribute)+1;
    cred[i].issuer_attribute = cd->credential->issuer_attribute;
    cred[i].expiration = cd->credential->expiration;
    cred[i].signature = cd->credential->signature;
    cd = cd->next;
  }
  size = GNUNET_CREDENTIAL_delegation_chain_get_size (vrh->delegation_chain_size,
                                                      dd,
                                                      vrh->cred_chain_size,
                                                      cred);
  env = GNUNET_MQ_msg_extra (rmsg,
                             size,
                             GNUNET_MESSAGE_TYPE_CREDENTIAL_VERIFY_RESULT);
  //Assign id so that client can find associated request
  rmsg->id = vrh->request_id;
  rmsg->d_count = htonl (vrh->delegation_chain_size);
  rmsg->c_count = htonl (vrh->cred_chain_size);

  if (0 < vrh->cred_chain_size)
    rmsg->cred_found = htonl (GNUNET_YES);
  else
    rmsg->cred_found = htonl (GNUNET_NO);

  GNUNET_assert (-1 != 
                 GNUNET_CREDENTIAL_delegation_chain_serialize (vrh->delegation_chain_size,
                                                               dd,
                                                               vrh->cred_chain_size,
                                                               cred,
                                                               size,
                                                               (char*)&rmsg[1]));

  GNUNET_MQ_send (GNUNET_SERVICE_client_get_mq(vrh->client),
                  env);
  GNUNET_CONTAINER_DLL_remove (vrh_head, vrh_tail, vrh);
  cleanup_handle(vrh);

  GNUNET_STATISTICS_update (statistics,
                            "Completed verifications", 1,
                            GNUNET_NO);
}


static void
backward_resolution (void* cls,
                     uint32_t rd_count,
                     const struct GNUNET_GNSRECORD_Data *rd)
{

  struct VerifyRequestHandle *vrh;
  const struct GNUNET_CREDENTIAL_DelegationRecord *sets;
  struct CredentialRecordEntry *cred_pointer;
  struct DelegationSetQueueEntry *current_set;
  struct DelegationSetQueueEntry *ds_entry;
  struct DelegationSetQueueEntry *tmp_set;
  struct DelegationQueueEntry *dq_entry;
  char *expanded_attr;
  char *lookup_attribute;


  current_set = cls;
  current_set->lookup_request = NULL;
  vrh = current_set->handle;
  vrh->pending_lookups--;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Got %d attrs\n", rd_count);

  // Each OR
  for (uint32_t i=0; i < rd_count; i++) 
  {
    if (GNUNET_GNSRECORD_TYPE_ATTRIBUTE != rd[i].record_type)
      continue;

    sets = rd[i].data;
    struct GNUNET_CREDENTIAL_DelegationSet set[ntohl(sets->set_count)];
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Found new attribute delegation with %d sets. Creating new Job...\n",
                ntohl (sets->set_count));

    if (GNUNET_OK !=GNUNET_CREDENTIAL_delegation_set_deserialize (GNUNET_ntohll(sets->data_size),
                                                                  (const char*)&sets[1],
                                                                  ntohl(sets->set_count),
                                                                  set))
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "Failed to deserialize!\n");
      continue;
    }
    dq_entry = GNUNET_new (struct DelegationQueueEntry);
    dq_entry->required_solutions = ntohl(sets->set_count);
    dq_entry->parent_set = current_set;
    GNUNET_CONTAINER_DLL_insert (current_set->queue_entries_head,
                                 current_set->queue_entries_tail,
                                 dq_entry);
    // Each AND
    for (uint32_t j=0; j<ntohl(sets->set_count); j++)
    {
      ds_entry = GNUNET_new (struct DelegationSetQueueEntry);
      if (NULL != current_set->attr_trailer)
      {
        if (0 == set[j].subject_attribute_len)
        {
          GNUNET_asprintf (&expanded_attr,
                           "%s",
                           current_set->attr_trailer);

        } else {
          GNUNET_asprintf (&expanded_attr,
                           "%s.%s",
                           set[j].subject_attribute,
                           current_set->attr_trailer);
        }
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "Expanded to %s\n", expanded_attr);
        ds_entry->unresolved_attribute_delegation = expanded_attr;
      } else {
        if (0 != set[j].subject_attribute_len)
        {
          GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                      "Not Expanding %s\n", set[j].subject_attribute);
          ds_entry->unresolved_attribute_delegation = GNUNET_strdup (set[j].subject_attribute);
        }
      }

      //Add a credential chain entry
      ds_entry->delegation_chain_entry = GNUNET_new (struct DelegationChainEntry);
      ds_entry->delegation_chain_entry->subject_key = set[j].subject_key;
      ds_entry->issuer_key = GNUNET_new (struct GNUNET_CRYPTO_EcdsaPublicKey);
      GNUNET_memcpy (ds_entry->issuer_key,
                     &set[j].subject_key,
                     sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));
      if (0 < set[j].subject_attribute_len)
        ds_entry->delegation_chain_entry->subject_attribute =  GNUNET_strdup (set[j].subject_attribute);
      ds_entry->delegation_chain_entry->issuer_key = *current_set->issuer_key;
      ds_entry->delegation_chain_entry->issuer_attribute = GNUNET_strdup (current_set->lookup_attribute);

      ds_entry->parent_queue_entry = dq_entry; //current_delegation;
      GNUNET_CONTAINER_DLL_insert (dq_entry->set_entries_head,
                                   dq_entry->set_entries_tail,
                                   ds_entry);

      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Checking for cred match\n");
      /**
       * Check if this delegation already matches one of our credentials
       */
      for(cred_pointer = vrh->cred_chain_head; cred_pointer != NULL; 
          cred_pointer = cred_pointer->next)
      {
        if(0 != memcmp (&set->subject_key, 
                        &cred_pointer->credential->issuer_key,
                        sizeof(struct GNUNET_CRYPTO_EcdsaPublicKey)))
          continue;
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "Checking if %s matches %s\n",
                    ds_entry->unresolved_attribute_delegation,
                    cred_pointer->credential->issuer_attribute);

        if (0 != strcmp (ds_entry->unresolved_attribute_delegation,
                         cred_pointer->credential->issuer_attribute))
          continue;

        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "Found issuer\n");
        cred_pointer->refcount++;
        //Backtrack
        for (tmp_set = ds_entry;
             NULL != tmp_set->parent_queue_entry;
             tmp_set = tmp_set->parent_queue_entry->parent_set)
        {
          tmp_set->parent_queue_entry->required_solutions--;
          if (NULL != tmp_set->delegation_chain_entry)
          {
            vrh->delegation_chain_size++;
            GNUNET_CONTAINER_DLL_insert (vrh->delegation_chain_head,
                                         vrh->delegation_chain_tail,
                                         tmp_set->delegation_chain_entry);
          }
          if (0 < tmp_set->parent_queue_entry->required_solutions)
            break;
        }

        if (NULL == tmp_set->parent_queue_entry)
        {
          GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                      "All solutions found\n");
          //Found match
          send_lookup_response (vrh);
          return;
        }
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "Not all solutions found yet.\n");
        continue;

      }
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Building new lookup request from %s\n",
                  ds_entry->unresolved_attribute_delegation);
      //Continue with backward resolution
      char issuer_attribute_name[strlen (ds_entry->unresolved_attribute_delegation)+1];
      strcpy (issuer_attribute_name,
              ds_entry->unresolved_attribute_delegation);
      char *next_attr = strtok (issuer_attribute_name, ".");
      if (NULL == next_attr)
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "Failed to parse next attribute\n");
        continue;
      }
      GNUNET_asprintf (&lookup_attribute,
                       "%s.gnu",
                       next_attr);
      GNUNET_asprintf (&ds_entry->lookup_attribute,
                       "%s",
                       next_attr);
      if (strlen (next_attr) == strlen (ds_entry->unresolved_attribute_delegation))
      {
        ds_entry->attr_trailer = NULL;
      } else {
        next_attr += strlen (next_attr) + 1;
        ds_entry->attr_trailer = GNUNET_strdup (next_attr);
      }

      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Looking up %s\n", ds_entry->lookup_attribute);
      if (NULL != ds_entry->attr_trailer)
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "%s still to go...\n", ds_entry->attr_trailer);

      vrh->pending_lookups++;
      ds_entry->handle = vrh;
      ds_entry->lookup_request = GNUNET_GNS_lookup (gns,
                                                    lookup_attribute,
                                                    ds_entry->issuer_key, //issuer_key,
                                                    GNUNET_GNSRECORD_TYPE_ATTRIBUTE,
                                                    GNUNET_GNS_LO_DEFAULT,
                                                    &backward_resolution,
                                                    ds_entry);
      GNUNET_free (lookup_attribute);
    }
  }

  if(0 == vrh->pending_lookups)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "We are all out of attributes...\n");
    send_lookup_response (vrh);
    return;

  }
} 


/**
 * Result from GNS lookup.
 *
 * @param cls the closure (our client lookup handle)
 */
static void
delegation_chain_resolution_start (void* cls)
{
  struct VerifyRequestHandle *vrh = cls;
  struct DelegationSetQueueEntry *ds_entry;
  struct CredentialRecordEntry *cr_entry;
  vrh->lookup_request = NULL;

  if (0 == vrh->cred_chain_size)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "No credentials found\n");
    send_lookup_response (vrh);
    return;
  }

  for (cr_entry = vrh->cred_chain_head; cr_entry != NULL; cr_entry = cr_entry->next)
  {
    if (0 != memcmp (&cr_entry->credential->issuer_key,
                     &vrh->issuer_key,
                     sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)))
      continue;
    if (0 != strcmp (cr_entry->credential->issuer_attribute, vrh->issuer_attribute))
      continue;
    cr_entry->refcount++;
    //Found match prematurely
    send_lookup_response (vrh);
    return;

  }

  /**
   * Check for attributes from the issuer and follow the chain 
   * till you get the required subject's attributes
   */
  char issuer_attribute_name[strlen (vrh->issuer_attribute) + strlen (".gnu") + 1];
  strcpy (issuer_attribute_name,
          vrh->issuer_attribute);
  strcpy (issuer_attribute_name + strlen (vrh->issuer_attribute),
          ".gnu");
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Looking up %s\n", issuer_attribute_name);
  ds_entry = GNUNET_new (struct DelegationSetQueueEntry);
  ds_entry->issuer_key = GNUNET_new (struct GNUNET_CRYPTO_EcdsaPublicKey);
  memcpy (ds_entry->issuer_key,
          &vrh->issuer_key,
          sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));
  ds_entry->issuer_attribute = GNUNET_strdup (vrh->issuer_attribute);
  ds_entry->handle = vrh;
  ds_entry->lookup_attribute = GNUNET_strdup (vrh->issuer_attribute);
  vrh->root_set = ds_entry;
  vrh->pending_lookups = 1;
  //Start with backward resolution
  ds_entry->lookup_request = GNUNET_GNS_lookup (gns,
                                                issuer_attribute_name,
                                                &vrh->issuer_key, //issuer_key,
                                                GNUNET_GNSRECORD_TYPE_ATTRIBUTE,
                                                GNUNET_GNS_LO_DEFAULT,
                                                &backward_resolution,
                                                ds_entry);
}

static int
check_verify (void *cls,
              const struct VerifyMessage *v_msg)
{
  size_t msg_size;
  const char* attr;

  msg_size = ntohs (v_msg->header.size);
  if (msg_size < sizeof (struct VerifyMessage))
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  if (ntohs (v_msg->issuer_attribute_len) > GNUNET_CREDENTIAL_MAX_LENGTH)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  attr = (const char *) &v_msg[1];

  if ( strlen (attr) > GNUNET_CREDENTIAL_MAX_LENGTH)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}

static void
handle_verify (void *cls,
               const struct VerifyMessage *v_msg) 
{
  struct VerifyRequestHandle *vrh;
  struct GNUNET_SERVICE_Client *client = cls;
  struct CredentialRecordEntry *cr_entry;
  uint32_t credentials_count;
  uint32_t credential_data_size;
  char attr[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  char issuer_attribute[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  char *attrptr = attr;
  char *credential_data;
  const char *utf_in;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received VERIFY message\n");
  utf_in = (const char *) &v_msg[1];
  GNUNET_STRINGS_utf8_tolower (utf_in, attrptr);
  GNUNET_memcpy (issuer_attribute, attr, ntohs (v_msg->issuer_attribute_len));
  issuer_attribute[ntohs (v_msg->issuer_attribute_len)] = '\0';
  vrh = GNUNET_new (struct VerifyRequestHandle);
  GNUNET_CONTAINER_DLL_insert (vrh_head, vrh_tail, vrh);
  vrh->client = client;
  vrh->request_id = v_msg->id;
  vrh->issuer_key = v_msg->issuer_key;
  vrh->subject_key = v_msg->subject_key;
  vrh->issuer_attribute = GNUNET_strdup (issuer_attribute);
  if (0 == strlen (issuer_attribute))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, 
                "No issuer attribute provided!\n");
    send_lookup_response (vrh);
    return;
  }
  /**
   * First, collect credentials
   * TODO: cleanup!
   */
  credentials_count = ntohl(v_msg->c_count);
  credential_data_size = ntohs (v_msg->header.size) 
    - sizeof (struct VerifyMessage)
    - ntohs (v_msg->issuer_attribute_len)
    - 1;
  struct GNUNET_CREDENTIAL_Credential credentials[credentials_count];
  credential_data = (char*)&v_msg[1] + ntohs (v_msg->issuer_attribute_len) + 1;
  if (GNUNET_OK != GNUNET_CREDENTIAL_credentials_deserialize (credential_data_size,
                                                              credential_data,
                                                              credentials_count,
                                                              credentials))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, 
                "Cannot deserialize credentials!\n");
    send_lookup_response (vrh);
    return;
  }

  for (uint32_t i=0;i<credentials_count;i++) {
    cr_entry = GNUNET_new (struct CredentialRecordEntry);
    cr_entry->credential = GNUNET_malloc (sizeof (struct GNUNET_CREDENTIAL_Credential) +
                                          credentials[i].issuer_attribute_len);
    GNUNET_memcpy (cr_entry->credential,
                   &credentials[i],
                   sizeof (struct GNUNET_CREDENTIAL_Credential));
    GNUNET_memcpy (&cr_entry->credential[1],
                   credentials[i].issuer_attribute,
                   credentials[i].issuer_attribute_len);
    cr_entry->credential->issuer_attribute = (char*)&cr_entry->credential[1];
    GNUNET_CONTAINER_DLL_insert_tail (vrh->cred_chain_head,
                                      vrh->cred_chain_tail,
                                      cr_entry);
    vrh->cred_chain_size++;
  }

  delegation_chain_resolution_start (vrh);

}

static void
handle_cred_collection_error_cb (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Got disconnected from namestore database.\n");
  vrh->cred_collection_iter = NULL;
  send_lookup_response (vrh);
}

static void
collect_next (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  vrh->collect_next_task = NULL;
  GNUNET_assert (NULL != vrh->cred_collection_iter);
  GNUNET_NAMESTORE_zone_iterator_next (vrh->cred_collection_iter);
}

static void
handle_cred_collection_cb (void *cls,
                           const struct GNUNET_CRYPTO_EcdsaPrivateKey *key,
                           const char *label,
                           unsigned int rd_count,
                           const struct GNUNET_GNSRECORD_Data *rd)
{
  struct VerifyRequestHandle *vrh = cls;
  struct GNUNET_CREDENTIAL_Credential *crd;
  struct CredentialRecordEntry *cr_entry;
  int cred_record_count;

  cred_record_count = 0;
  for (uint32_t i=0; i < rd_count; i++)
  {
    if (GNUNET_GNSRECORD_TYPE_CREDENTIAL != rd[i].record_type)
      continue;
    cred_record_count++;
    crd = GNUNET_CREDENTIAL_credential_deserialize (rd[i].data,
                                                    rd[i].data_size);
    if (NULL == crd)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                  "Invalid credential found\n");
      continue;
    }
    cr_entry = GNUNET_new (struct CredentialRecordEntry);
    cr_entry->credential = crd;
    GNUNET_CONTAINER_DLL_insert_tail (vrh->cred_chain_head,
                                      vrh->cred_chain_tail,
                                      cr_entry);
    vrh->cred_chain_size++;
  }
  vrh->collect_next_task = GNUNET_SCHEDULER_add_now (&collect_next,
                                                     vrh);
}

static void
handle_cred_collection_finished_cb (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Done collecting credentials.\n");
  vrh->cred_collection_iter = NULL;
  delegation_chain_resolution_start (vrh);
}

static void
handle_collect (void *cls,
                const struct CollectMessage *c_msg) 
{
  char attr[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  char issuer_attribute[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  struct VerifyRequestHandle *vrh;
  struct GNUNET_SERVICE_Client *client = cls;
  char *attrptr = attr;
  const char *utf_in;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received COLLECT message\n");

  utf_in = (const char *) &c_msg[1];
  GNUNET_STRINGS_utf8_tolower (utf_in, attrptr);

  GNUNET_memcpy (issuer_attribute, attr, ntohs (c_msg->issuer_attribute_len));
  issuer_attribute[ntohs (c_msg->issuer_attribute_len)] = '\0';
  vrh = GNUNET_new (struct VerifyRequestHandle);
  GNUNET_CONTAINER_DLL_insert (vrh_head, vrh_tail, vrh);
  vrh->client = client;
  vrh->request_id = c_msg->id;
  vrh->issuer_key = c_msg->issuer_key;
  GNUNET_CRYPTO_ecdsa_key_get_public (&c_msg->subject_key,
                                      &vrh->subject_key);
  vrh->issuer_attribute = GNUNET_strdup (issuer_attribute);

  if (0 == strlen (issuer_attribute))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, 
                "No issuer attribute provided!\n");
    send_lookup_response (vrh);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Getting credentials for subject\n");
  /**
   * First, get attribute from subject
   */
  vrh->cred_collection_iter = GNUNET_NAMESTORE_zone_iteration_start (namestore,
                                                                     &c_msg->subject_key,
                                                                     &handle_cred_collection_error_cb,
                                                                     vrh,
                                                                     &handle_cred_collection_cb,
                                                                     vrh,
                                                                     &handle_cred_collection_finished_cb,
                                                                     vrh);
}


static int
check_collect (void *cls,
               const struct CollectMessage *c_msg)
{
  size_t msg_size;
  const char* attr;

  msg_size = ntohs (c_msg->header.size);
  if (msg_size < sizeof (struct CollectMessage))
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  if (ntohs (c_msg->issuer_attribute_len) > GNUNET_CREDENTIAL_MAX_LENGTH)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  attr = (const char *) &c_msg[1];

  if ( ('\0' != attr[msg_size - sizeof (struct CollectMessage) - 1]) ||
       (strlen (attr) > GNUNET_CREDENTIAL_MAX_LENGTH) )
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}

static void
client_disconnect_cb (void *cls,
                      struct GNUNET_SERVICE_Client *client,
                      void *app_ctx)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Client %p disconnected\n",
              client);
}

static void *
client_connect_cb (void *cls,
                   struct GNUNET_SERVICE_Client *client,
                   struct GNUNET_MQ_Handle *mq)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Client %p connected\n",
              client);
  return client;
}

/**
 * Process Credential requests.
 *
 * @param cls closure
 * @param c configuration to use
 * @param handle service handle
 */
static void
run (void *cls,
     const struct GNUNET_CONFIGURATION_Handle *c,
     struct GNUNET_SERVICE_Handle *handle)
{

  gns = GNUNET_GNS_connect (c);
  if (NULL == gns)
  {
    fprintf (stderr,
             _("Failed to connect to GNS\n"));
  }
  namestore = GNUNET_NAMESTORE_connect (c);
  if (NULL == namestore)
  {
    fprintf (stderr,
             _("Failed to connect to namestore\n"));
  }

  statistics = GNUNET_STATISTICS_create ("credential", c);
  GNUNET_SCHEDULER_add_shutdown (&shutdown_task, NULL);
}


/**
 * Define "main" method using service macro
 */
GNUNET_SERVICE_MAIN
("credential",
 GNUNET_SERVICE_OPTION_NONE,
 &run,
 &client_connect_cb,
 &client_disconnect_cb,
 NULL,
 GNUNET_MQ_hd_var_size (verify,
                        GNUNET_MESSAGE_TYPE_CREDENTIAL_VERIFY,
                        struct VerifyMessage,
                        NULL),
 GNUNET_MQ_hd_var_size (collect,
                        GNUNET_MESSAGE_TYPE_CREDENTIAL_COLLECT,
                        struct CollectMessage,
                        NULL),
 GNUNET_MQ_handler_end());

/* end of gnunet-service-credential.c */
