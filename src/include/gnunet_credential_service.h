/*
      This file is part of GNUnet
      Copyright (C) 2012-2014 GNUnet e.V.

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
 * @author Adnan Husain
 *
 * @file
 * API to the Credential service
 *
 * @defgroup credential  Credential service
 * Credentials
 *
 * @{
 */
#ifndef GNUNET_CREDENTIAL_SERVICE_H
#define GNUNET_CREDENTIAL_SERVICE_H

#include "gnunet_util_lib.h"
#include "gnunet_gns_service.h"
#include "gnunet_identity_service.h"

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif


/**
 * Connection to the Credential service.
 */
struct GNUNET_CREDENTIAL_Handle;

/**
 * Handle to control a lookup operation.
 */
struct GNUNET_CREDENTIAL_Request;

/*
* Enum used for checking whether the issuer has the authority to issue credentials or is just a subject
*/
enum GNUNET_CREDENTIAL_CredentialFlags {

  //Subject had credentials before, but have been revoked now
  GNUNET_CREDENTIAL_FLAG_REVOKED=0,

  //Subject flag indicates that the subject is a holder of this credential and may present it as such
  GNUNET_CREDENTIAL_FLAG_SUBJECT=1,

  //Issuer flag is used to signify that the subject is allowed to issue this credential and delegate issuance
  GNUNET_CREDENTIAL_FLAG_ISSUER=2

};

GNUNET_NETWORK_STRUCT_BEGIN
/**
 * The credential record 
 */
struct GNUNET_CREDENTIAL_CredentialRecordData {
  
  /**
   * The signature for this credential by the issuer
   */
  struct GNUNET_CRYPTO_EcdsaSignature sig;
  
  /**
   * Signature meta
   */
  struct GNUNET_CRYPTO_EccSignaturePurpose purpose;

  /**
   * Public key of the issuer
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey issuer_key;
  
  /**
   * Public key of the subject this credential was issued to
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey subject_key;
  
  /**
   * Expiration time of this credential
   */
  uint64_t expiration GNUNET_PACKED;
  
    /**
   * Followed by the attribute string
   */
};


/**
 * The attribute delegation record
*/
struct GNUNET_CREDENTIAL_AttributeRecordData {
  
  /**
   * Public key of the subject this attribute was delegated to
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey subject_key;
  
  /**
   * Followed by the attribute that was delegated to as string
   * May be empty
   */
};



GNUNET_NETWORK_STRUCT_END



/**
 * Initialize the connection with the Credential service.
 *
 * @param cfg configuration to use
 * @return handle to the Credential service, or NULL on error
 */
struct GNUNET_CREDENTIAL_Handle *
GNUNET_CREDENTIAL_connect (const struct GNUNET_CONFIGURATION_Handle *cfg);


/**
 * Shutdown connection with the Credentail service.
 *
 * @param handle connection to shut down
 */
void
GNUNET_CREDENTIAL_disconnect (struct GNUNET_CREDENTIAL_Handle *handle);


/**
 * Iterator called on obtained result for an attribute verification.
 *
 * @param cls closure
 * @param issuer the issuer of the attribute NULL if verification failed
 * @param result the result of the verification
 * @param rd the records in reply
 */
typedef void (*GNUNET_CREDENTIAL_VerifyResultProcessor) (void *cls,
						  struct GNUNET_CREDENTIAL_CredentialRecordData *credential,
              uint32_t delegation_length,
              struct GNUNET_CREDENTIAL_AttributeRecordData *delegation_chain);

/**
 * Iterator called on obtained result for an attribute delegation.
 *
 * @param cls closure
 * @param success GNUNET_YES if successful
 * @param result the record data that can be handed to the subject
 */
typedef void (*GNUNET_CREDENTIAL_DelegateResultProcessor) (void *cls,
						  uint32_t success);

/**
 * Iterator called on obtained result for an attribute delegation removal.
 *
 * @param cls closure
 * @param success GNUNET_YES if successful
 * @param result the record data that can be handed to the subject
 */
typedef void (*GNUNET_CREDENTIAL_RemoveDelegateResultProcessor) (void *cls,
						  uint32_t success);




/**
 * Performs attribute verification.
 * Checks if there is a delegation chain from
 * attribute ``issuer_attribute'' issued by the issuer
 * with public key ``issuer_key'' maps to the attribute
 * ``subject_attribute'' claimed by the subject with key
 * ``subject_key''
 *
 * @param handle handle to the Credential service
 * @param issuer_key the issuer public key
 * @param issuer_attribute the issuer attribute
 * @param subject_key the subject public key
 * @param subject_attribute the attribute claimed by the subject
 * @param proc function to call on result
 * @param proc_cls closure for processor
 * @return handle to the queued request
 */
struct GNUNET_CREDENTIAL_Request*
GNUNET_CREDENTIAL_verify (struct GNUNET_CREDENTIAL_Handle *handle,
                          const struct GNUNET_CRYPTO_EcdsaPublicKey *issuer_key,
                          const char *issuer_attribute,
                          const struct GNUNET_CRYPTO_EcdsaPublicKey *subject_key,
                          const char *subject_attribute,
                          GNUNET_CREDENTIAL_VerifyResultProcessor proc,
                          void *proc_cls);

/**
 * Delegate an attribute
 *
 * @param handle handle to the Credential service
 * @param issuer the ego that should be used to delegate the attribute
 * @param attribute the name of the attribute to delegate
 * @param subject the subject of the delegation
 * @param delegated_attribute the name of the attribute that is delegated to
 * @return handle to the queued request
 */
struct GNUNET_CREDENTIAL_Request *
GNUNET_CREDENTIAL_add_delegation (struct GNUNET_CREDENTIAL_Handle *handle,
                                  struct GNUNET_IDENTITY_Ego *issuer,
                                  const char *attribute,
                                  struct GNUNET_CRYPTO_EcdsaPublicKey *subject,
                                  const char *delegated_attribute,
                                  GNUNET_CREDENTIAL_DelegateResultProcessor proc,
                                  void *proc_cls);

/**
 * Remove a delegation
 *
 * @param handle handle to the Credential service
 * @param issuer the ego that was used to delegate the attribute
 * @param attribute the name of the attribute that is delegated
 * @return handle to the queued request
 */
struct GNUNET_CREDENTIAL_Request *
GNUNET_CREDENTIAL_remove_delegation (struct GNUNET_CREDENTIAL_Handle *handle,
                                  struct GNUNET_IDENTITY_Ego *issuer,
                                  const char *attribute,
                                  GNUNET_CREDENTIAL_RemoveDelegateResultProcessor proc,
                                  void *proc_cls);



/**
 * Issue an attribute to a subject
 *
 * @param handle handle to the Credential service
 * @param issuer the ego that should be used to issue the attribute
 * @param subject the subject of the attribute
 * @param attribute the name of the attribute
 * @param expiration the TTL of the credential
 * @return handle to the queued request
 */
struct GNUNET_CREDENTIAL_CredentialRecordData *
GNUNET_CREDENTIAL_issue (struct GNUNET_CREDENTIAL_Handle *handle,
                         const struct GNUNET_CRYPTO_EcdsaPrivateKey *issuer,
                         struct GNUNET_CRYPTO_EcdsaPublicKey *subject,
                         const char *attribute,
                         struct GNUNET_TIME_Absolute *expiration);


/**
 * Remove a credential
 *
 * @param handle handle to the Credential service
 * @param issuer the identity that issued the credential
 * @param subject the subject of the credential
 * @param credential the name of the credential
 * @return handle to the queued request
 */
/**
  struct GNUNET_CREDENTIAL_IssueRequest *
  GNUNET_CREDENTIAL_remove (struct GNUNET_CREDENTIAL_Handle *handle,
  struct GNUNET_IDENTITY_Ego *issuer,
  struct GNUNET_IDENTITY_Ego *subject,
  const char *credential,
  GNUNET_CREDENTIAL_IssueResultProcessor proc,
  void *proc_cls);
  */


/**
 * Cancel pending lookup request
 *
 * @param lr the lookup request to cancel
 */
void
GNUNET_CREDENTIAL_verify_cancel (struct GNUNET_CREDENTIAL_Request *vr);


#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

#endif

/** @} */  /* end of group */
