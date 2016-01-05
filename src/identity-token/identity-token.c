/*
      This file is part of GNUnet
      Copyright (C) 2010-2015 Christian Grothoff (and other contributing authors)

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
 * @file identity-token/identity-token.c
 * @brief helper library to manage identity tokens
 * @author Martin Schanzenbach
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_signatures.h"
#include "identity-token.h"
#include <jansson.h>


/**
 * Crypto helper functions
 */

static int
create_sym_key_from_ecdh(const struct GNUNET_HashCode *new_key_hash,
                         struct GNUNET_CRYPTO_SymmetricSessionKey *skey,
                         struct GNUNET_CRYPTO_SymmetricInitializationVector *iv)
{
  struct GNUNET_CRYPTO_HashAsciiEncoded new_key_hash_str;

  GNUNET_CRYPTO_hash_to_enc (new_key_hash,
                             &new_key_hash_str);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Creating symmetric rsa key from %s\n", (char*)&new_key_hash_str);
  static const char ctx_key[] = "gnuid-aes-ctx-key";
  GNUNET_CRYPTO_kdf (skey, sizeof (struct GNUNET_CRYPTO_SymmetricSessionKey),
                     new_key_hash, sizeof (struct GNUNET_HashCode),
                     ctx_key, strlen (ctx_key),
                     NULL, 0);
  static const char ctx_iv[] = "gnuid-aes-ctx-iv";
  GNUNET_CRYPTO_kdf (iv, sizeof (struct GNUNET_CRYPTO_SymmetricInitializationVector),
                     new_key_hash, sizeof (struct GNUNET_HashCode),
                     ctx_iv, strlen (ctx_iv),
                     NULL, 0);
  return GNUNET_OK;
}

/**
 * Decrypts metainfo part from a token code
 */
static int
decrypt_str_ecdhe (const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                   const struct GNUNET_CRYPTO_EcdhePublicKey *ecdh_key,
                   const char *enc_str,
                   size_t enc_str_len,
                   char **result_str)
{
  struct GNUNET_HashCode new_key_hash;
  struct GNUNET_CRYPTO_SymmetricSessionKey enc_key;
  struct GNUNET_CRYPTO_SymmetricInitializationVector enc_iv;

  char *str_buf = GNUNET_malloc (enc_str_len);
  size_t str_size;

  //Calculate symmetric key from ecdh parameters
  GNUNET_assert (GNUNET_OK == GNUNET_CRYPTO_ecdsa_ecdh (priv_key,
                                                        ecdh_key,
                                                        &new_key_hash));

  create_sym_key_from_ecdh (&new_key_hash,
                            &enc_key,
                            &enc_iv);

  str_size = GNUNET_CRYPTO_symmetric_decrypt (enc_str,
                                              enc_str_len,
                                              &enc_key,
                                              &enc_iv,
                                              str_buf);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Decrypted bytes: %d Expected bytes: %d\n", str_size, enc_str_len);
  if (-1 == str_size)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "ECDH invalid\n");
    return GNUNET_SYSERR;
  }
  *result_str = GNUNET_malloc (str_size+1);
  memcpy (*result_str, str_buf, str_size);
  (*result_str)[str_size] = '\0';
  GNUNET_free (str_buf);
  return GNUNET_OK;

}

/**
 * Encrypt string using pubkey and ECDHE
 * Returns ECDHE pubkey to be used for decryption
 */
static int
encrypt_str_ecdhe (const char *data,
                   const struct GNUNET_CRYPTO_EcdsaPublicKey *pub_key,
                   char **enc_data_str,
                   struct GNUNET_CRYPTO_EcdhePublicKey *ecdh_pubkey)
{
  struct GNUNET_CRYPTO_EcdhePrivateKey *ecdh_privkey;
  struct GNUNET_CRYPTO_SymmetricSessionKey skey;
  struct GNUNET_CRYPTO_SymmetricInitializationVector iv;
  struct GNUNET_HashCode new_key_hash;
  
  // ECDH keypair E = eG
  ecdh_privkey = GNUNET_CRYPTO_ecdhe_key_create();
  GNUNET_CRYPTO_ecdhe_key_get_public (ecdh_privkey,
                                      ecdh_pubkey);

  *enc_data_str = GNUNET_malloc (strlen (data));

  // Derived key K = H(eB)
  GNUNET_assert (GNUNET_OK == GNUNET_CRYPTO_ecdh_ecdsa (ecdh_privkey,
                                                        pub_key,
                                                        &new_key_hash));
  create_sym_key_from_ecdh(&new_key_hash, &skey, &iv);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Encrypting string %s\n", data);
  GNUNET_CRYPTO_symmetric_encrypt (data, strlen (data),
                                   &skey, &iv,
                                   *enc_data_str);
  GNUNET_free (ecdh_privkey);
  return GNUNET_OK;
}




/**
 * Identity Token API
 */


/**
 * Create an Identity Token
 *
 * @param type the JSON API resource type
 * @param id the JSON API resource id
 * @return a new JSON API resource or NULL on error.
 */
struct IdentityToken*
identity_token_create (const char* issuer,
                       const char* audience)
{
  struct IdentityToken *token;

  token = GNUNET_malloc (sizeof (struct IdentityToken));

  token->header = json_object();
  token->payload = json_object();

  json_object_set_new (token->header, "alg", json_string ("ED512"));
  json_object_set_new (token->header, "typ", json_string ("JWT"));

  json_object_set_new (token->payload, "iss", json_string (issuer));
  json_object_set_new (token->payload, "aud", json_string (audience));

  GNUNET_STRINGS_string_to_data  (audience,
                                  strlen (audience),
                                  &token->aud_key,
                                  sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));

  return token;
}

void
identity_token_destroy (struct IdentityToken *token)
{
  json_decref (token->header);
  json_decref (token->payload);
  GNUNET_free (token);
}

void
identity_token_add_attr (const struct IdentityToken *token,
                         const char* key,
                         const char* value)
{
  GNUNET_assert (NULL != token);
  GNUNET_assert (NULL != token->payload);

  json_object_set_new (token->payload, key, json_string (value));
}

void
identity_token_add_json (const struct IdentityToken *token,
                         const char* key,
                         json_t* value)
{
  GNUNET_assert (NULL != token);
  GNUNET_assert (NULL != token->payload);

  json_object_set_new (token->payload, key, value);
}


int
identity_token_parse (const char* raw_data,
                      const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                      struct IdentityToken **result)
{
  char *ecdh_pubkey_str;
  char *enc_token_str;
  char *tmp_buf;
  char *token_str;
  char *enc_token;
  char *header;
  char *header_base64;
  char *payload;
  char *payload_base64;
  size_t enc_token_len;
  json_error_t err_json;
  struct GNUNET_CRYPTO_EcdhePublicKey ecdh_pubkey;

  GNUNET_asprintf (&tmp_buf, "%s", raw_data);
  ecdh_pubkey_str = strtok (tmp_buf, ",");
  enc_token_str = strtok (NULL, ",");

  GNUNET_STRINGS_string_to_data (ecdh_pubkey_str,
                                 strlen (ecdh_pubkey_str),
                                 &ecdh_pubkey,
                                 sizeof (struct GNUNET_CRYPTO_EcdhePublicKey));
  enc_token_len = GNUNET_STRINGS_base64_decode (enc_token_str,
                                                strlen (enc_token_str),
                                                &enc_token);
  if (GNUNET_OK != decrypt_str_ecdhe (priv_key,
                                      &ecdh_pubkey,
                                      enc_token,
                                      enc_token_len,
                                      &token_str))
  {
    GNUNET_free (tmp_buf);
    GNUNET_free (enc_token);
    return GNUNET_SYSERR;
  }

  header_base64 = strtok (token_str, ".");
  payload_base64 = strtok (NULL, ".");

  GNUNET_STRINGS_base64_decode (header_base64,
                                strlen (header_base64),
                                &header);
  GNUNET_STRINGS_base64_decode (payload_base64,
                                strlen (payload_base64),
                                &payload);
  //TODO signature and aud key


  *result = GNUNET_malloc (sizeof (struct IdentityToken));
  (*result)->header = json_loads (header, JSON_DECODE_ANY, &err_json);
  (*result)->payload = json_loads (payload, JSON_DECODE_ANY, &err_json);
  GNUNET_free (enc_token);
  GNUNET_free (token_str);
  GNUNET_free (tmp_buf);
  GNUNET_free (payload);
  GNUNET_free (header);
  return GNUNET_OK;
}

int
identity_token_to_string (const struct IdentityToken *token,
                          const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                          char **result)
{
  char *payload_str;
  char *header_str;
  char *payload_base64;
  char *header_base64;
  char *padding;
  char *signature_target;
  char *signature_str;
  struct GNUNET_CRYPTO_EccSignaturePurpose *purpose;
  header_str = json_dumps (token->header, JSON_COMPACT);
  GNUNET_STRINGS_base64_encode (header_str,
                                strlen (header_str),
                                &header_base64);
  //Remove GNUNET padding of base64
  padding = strtok(header_base64, "=");
  while (NULL != padding)
    padding = strtok(NULL, "=");

  payload_str = json_dumps (token->payload, JSON_COMPACT);
  GNUNET_STRINGS_base64_encode (payload_str,
                                strlen (payload_str),
                                &payload_base64);

  //Remove GNUNET padding of base64
  padding = strtok(payload_base64, "=");
  while (NULL != padding)
    padding = strtok(NULL, "=");

  GNUNET_asprintf (&signature_target, "%s,%s", header_base64, payload_base64);
  purpose =
    GNUNET_malloc (sizeof (struct GNUNET_CRYPTO_EccSignaturePurpose) +
                   strlen (signature_target));
  purpose->size =
    htonl (strlen (signature_target) + sizeof (struct GNUNET_CRYPTO_EccSignaturePurpose));
  purpose->purpose = htonl(GNUNET_SIGNATURE_PURPOSE_GNUID_TOKEN);
  memcpy (&purpose[1], signature_target, strlen (signature_target));
  if (GNUNET_OK != GNUNET_CRYPTO_ecdsa_sign (priv_key,
                                             purpose,
                                             (struct GNUNET_CRYPTO_EcdsaSignature *)&token->signature))
  {
    GNUNET_free (signature_target);
    GNUNET_free (payload_str);
    GNUNET_free (header_str);
    GNUNET_free (payload_base64);
    GNUNET_free (header_base64);
    GNUNET_free (purpose);
    return GNUNET_SYSERR;
  }

  GNUNET_STRINGS_base64_encode ((const char*)&token->signature,
                                sizeof (struct GNUNET_CRYPTO_EcdsaSignature),
                                &signature_str);
  GNUNET_asprintf (result, "%s.%s.%s",
                   header_base64, payload_base64, signature_str);
  GNUNET_free (signature_target);
  GNUNET_free (payload_str);
  GNUNET_free (header_str);
  GNUNET_free (signature_str);
  GNUNET_free (payload_base64);
  GNUNET_free (header_base64);
  GNUNET_free (purpose);
  return GNUNET_OK;
}

int
identity_token_serialize (const struct IdentityToken *token,
                          const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                          char **result)
{
  char *token_str;
  char *enc_token;
  char *dh_key_str;
  char *enc_token_base64;
  struct GNUNET_CRYPTO_EcdhePublicKey ecdh_pubkey;

  GNUNET_assert (GNUNET_OK == identity_token_to_string (token,
                                                        priv_key,
                                                        &token_str));

  GNUNET_assert (GNUNET_OK == encrypt_str_ecdhe (token_str,
                                                 &token->aud_key,
                                                 &enc_token,
                                                 &ecdh_pubkey));
  GNUNET_STRINGS_base64_encode (enc_token,
                                strlen (token_str),
                                &enc_token_base64);
  dh_key_str = GNUNET_STRINGS_data_to_string_alloc (&ecdh_pubkey,
                                                    sizeof (struct GNUNET_CRYPTO_EcdhePublicKey));
  GNUNET_asprintf (result, "%s,%s", dh_key_str, enc_token_base64);
  GNUNET_free (dh_key_str);
  GNUNET_free (enc_token_base64);
  GNUNET_free (enc_token);
  GNUNET_free (token_str);
  return GNUNET_OK;
}

struct IdentityTokenCodePayload*
identity_token_code_payload_create (const char* nonce,
                                    const struct GNUNET_CRYPTO_EcdsaPublicKey* identity_pkey,
                                    const char* lbl_str)
{
  struct IdentityTokenCodePayload* payload;

  payload = GNUNET_malloc (sizeof (struct IdentityTokenCodePayload));
  GNUNET_asprintf (&payload->nonce, nonce, strlen (nonce));
  payload->identity_key = *identity_pkey;
  GNUNET_asprintf (&payload->label, lbl_str, strlen (lbl_str));
  return payload;
}

void
identity_token_code_payload_destroy (struct IdentityTokenCodePayload* payload)
{
  GNUNET_free (payload->nonce);
  GNUNET_free (payload->label);
  GNUNET_free (payload);
}

void
identity_token_code_payload_serialize (struct IdentityTokenCodePayload *payload,
                                       char **result)
{
  char* identity_key_str;

  identity_key_str = GNUNET_STRINGS_data_to_string_alloc (&payload->identity_key,
                                                          sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));

  GNUNET_asprintf (result, 
                   "{\"nonce\": \"%u\",\"identity\": \"%s\",\"label\": \"%s\"}",
                   payload->nonce, identity_key_str, payload->label);
  GNUNET_free (identity_key_str);

}


/**
 * Create the token code
 * The metadata is encrypted with a share ECDH derived secret using B (aud_key)
 * and e (ecdh_privkey)
 * The token_code also contains E (ecdh_pubkey) and a signature over the
 * metadata and E
 */
struct IdentityTokenCode*
identity_token_code_create (const char* nonce_str,
                            const struct GNUNET_CRYPTO_EcdsaPublicKey* identity_pkey,
                            const char* lbl_str,
                            const struct GNUNET_CRYPTO_EcdsaPublicKey *aud_key)
{
  struct IdentityTokenCode *token_code;
  struct IdentityTokenCodePayload *code_payload;

  token_code = GNUNET_malloc (sizeof (struct IdentityTokenCode));
  code_payload = identity_token_code_payload_create (nonce_str,
                                                     identity_pkey,
                                                     lbl_str);
  token_code->aud_key = *aud_key;
  token_code->payload = code_payload;


  return token_code;
}

void
identity_token_code_destroy (struct IdentityTokenCode *token_code)
{
  identity_token_code_payload_destroy (token_code->payload);
  GNUNET_free (token_code);
}

int
identity_token_code_serialize (struct IdentityTokenCode *identity_token_code,
                               const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                               char **result)
{
  char *code_payload_str;
  char *enc_token_code_payload;
  char *token_code_payload_str;
  char *token_code_sig_str;
  char *token_code_str;
  char *dh_key_str;
  char *write_ptr;

  struct GNUNET_CRYPTO_EccSignaturePurpose *purpose;

  identity_token_code_payload_serialize (identity_token_code->payload,
                                         &code_payload_str);

  GNUNET_assert (GNUNET_OK == encrypt_str_ecdhe (code_payload_str,
                                                 &identity_token_code->aud_key,
                                                 &enc_token_code_payload,
                                                 &identity_token_code->ecdh_pubkey));

  purpose = 
    GNUNET_malloc (sizeof (struct GNUNET_CRYPTO_EccSignaturePurpose) + 
                   sizeof (struct GNUNET_CRYPTO_EcdhePublicKey) + //E
                   strlen (code_payload_str)); // E_K (code_str)
  purpose->size = 
    htonl (sizeof (struct GNUNET_CRYPTO_EccSignaturePurpose) +
           sizeof (struct GNUNET_CRYPTO_EcdhePublicKey) +
           strlen (code_payload_str));
  purpose->purpose = htonl(GNUNET_SIGNATURE_PURPOSE_GNUID_TOKEN_CODE);
  write_ptr = (char*) &purpose[1];
  memcpy (write_ptr,
          &identity_token_code->ecdh_pubkey,
          sizeof (struct GNUNET_CRYPTO_EcdhePublicKey));
  write_ptr += sizeof (struct GNUNET_CRYPTO_EcdhePublicKey);
  memcpy (write_ptr, enc_token_code_payload, strlen (code_payload_str));
  GNUNET_assert (GNUNET_OK == GNUNET_CRYPTO_ecdsa_sign (priv_key,
                                                        purpose,
                                                        &identity_token_code->signature));
  GNUNET_STRINGS_base64_encode (enc_token_code_payload,
                                strlen (code_payload_str),
                                &token_code_payload_str);
  token_code_sig_str = GNUNET_STRINGS_data_to_string_alloc (&identity_token_code->signature,
                                                            sizeof (struct GNUNET_CRYPTO_EcdsaSignature));

  dh_key_str = GNUNET_STRINGS_data_to_string_alloc (&identity_token_code->ecdh_pubkey,
                                                    sizeof (struct GNUNET_CRYPTO_EcdhePublicKey));
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Using ECDH pubkey %s to encrypt\n", dh_key_str);
  GNUNET_asprintf (&token_code_str, "{\"meta\": \"%s\", \"ecdh\": \"%s\", \"signature\": \"%s\"}",
                   token_code_payload_str, dh_key_str, token_code_sig_str);
  GNUNET_STRINGS_base64_encode (token_code_str, strlen (token_code_str), result);
  GNUNET_free (dh_key_str);
  GNUNET_free (purpose);
  GNUNET_free (token_code_str);
  GNUNET_free (token_code_sig_str);
  GNUNET_free (code_payload_str);
  GNUNET_free (enc_token_code_payload);
  GNUNET_free (token_code_payload_str);
  return GNUNET_OK;
}

int
identity_token_code_payload_parse(const char *raw_data,
                                  const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                                  const struct GNUNET_CRYPTO_EcdhePublicKey *ecdhe_pkey,
                                  struct IdentityTokenCodePayload **result)
{
  const char* label_str;
  const char* nonce_str;
  const char* identity_key_str;

  json_t *root;
  json_t *label_json;
  json_t *identity_json;
  json_t *nonce_json;
  json_error_t err_json;
  char* meta_str;
  struct GNUNET_CRYPTO_EcdsaPublicKey id_pkey;

  if (GNUNET_OK != decrypt_str_ecdhe (priv_key,
                                      ecdhe_pkey,
                                      raw_data,
                                      strlen (raw_data),
                                      &meta_str))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Metadata decryption failed\n");
    return GNUNET_SYSERR;
  }

  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Metadata: %s\n", meta_str);
  root = json_loads (meta_str, JSON_DECODE_ANY, &err_json);
  if (!root)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Error parsing metadata: %s\n", err_json.text);
    GNUNET_free (meta_str);
    return GNUNET_SYSERR;
  }

  identity_json = json_object_get (root, "identity");
  if (!json_is_string (identity_json))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Error parsing metadata: %s\n", err_json.text);
    json_decref (root);
    GNUNET_free (meta_str);
    return GNUNET_SYSERR;
  }
  identity_key_str = json_string_value (identity_json);
  GNUNET_STRINGS_string_to_data (identity_key_str,
                                 strlen (identity_key_str),
                                 &id_pkey,
                                 sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));


  label_json = json_object_get (root, "label");
  if (!json_is_string (label_json))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Error parsing metadata: %s\n", err_json.text);
    json_decref (root);
    GNUNET_free (meta_str);
    return GNUNET_SYSERR;
  }

  label_str = json_string_value (label_json);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Found label: %s\n", label_str);

  nonce_json = json_object_get (root, "nonce");
  if (!json_is_string (label_json))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Error parsing metadata: %s\n", err_json.text);
    json_decref (root);
    GNUNET_free (meta_str);
    return GNUNET_SYSERR;
  }

  nonce_str = json_string_value (nonce_json);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Found nonce: %s\n", nonce_str);

  *result = identity_token_code_payload_create (nonce_str,
                                                (const struct GNUNET_CRYPTO_EcdsaPublicKey*)&id_pkey,
                                                label_str);
  GNUNET_free (meta_str);
  json_decref (root);
  return GNUNET_OK;

}

int
identity_token_code_parse (const char *raw_data,
                           const struct GNUNET_CRYPTO_EcdsaPrivateKey *priv_key,
                           struct IdentityTokenCode **result)
{
  const char* enc_meta_str;
  const char* ecdh_enc_str;
  const char* signature_enc_str;

  json_t *root;
  json_t *signature_json;
  json_t *ecdh_json;
  json_t *enc_meta_json;
  json_error_t err_json;
  char* enc_meta;
  char* token_code_decoded;
  char* write_ptr;
  size_t enc_meta_len;
  struct GNUNET_CRYPTO_EccSignaturePurpose *purpose;
  struct IdentityTokenCode *token_code;
  struct IdentityTokenCodePayload *token_code_payload;

  token_code_decoded = NULL;
  GNUNET_STRINGS_base64_decode (raw_data, strlen (raw_data), &token_code_decoded);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Token Code: %s\n", token_code_decoded);
  root = json_loads (token_code_decoded, JSON_DECODE_ANY, &err_json);
  if (!root)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "%s\n", err_json.text);
    return GNUNET_SYSERR;
  }

  signature_json = json_object_get (root, "signature");
  ecdh_json = json_object_get (root, "ecdh");
  enc_meta_json = json_object_get (root, "meta");

  signature_enc_str = json_string_value (signature_json);
  ecdh_enc_str = json_string_value (ecdh_json);
  enc_meta_str = json_string_value (enc_meta_json);

  token_code = GNUNET_malloc (sizeof (struct IdentityTokenCode));

  if (GNUNET_OK != GNUNET_STRINGS_string_to_data (ecdh_enc_str,
                                                  strlen (ecdh_enc_str),
                                                  &token_code->ecdh_pubkey,
                                                  sizeof  (struct GNUNET_CRYPTO_EcdhePublicKey)))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "ECDH PKEY %s invalid in metadata\n", ecdh_enc_str);
    json_decref (root);
    GNUNET_free (token_code);
    return GNUNET_SYSERR;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Using ECDH pubkey %s for metadata decryption\n", ecdh_enc_str);
  if (GNUNET_OK != GNUNET_STRINGS_string_to_data (signature_enc_str,
                                                  strlen (signature_enc_str),
                                                  &token_code->signature,
                                                  sizeof (struct GNUNET_CRYPTO_EcdsaSignature)))
  {
    json_decref (root);
    GNUNET_free (token_code_decoded);
    GNUNET_free (token_code);
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "ECDH signature invalid in metadata\n");
    return GNUNET_SYSERR;
  }

  enc_meta_len = GNUNET_STRINGS_base64_decode (enc_meta_str,
                                               strlen (enc_meta_str),
                                               &enc_meta);


  identity_token_code_payload_parse (enc_meta,
                                     priv_key,
                                     (const struct GNUNET_CRYPTO_EcdhePublicKey*)&token_code->ecdh_pubkey,
                                     &token_code_payload);

  token_code->payload = token_code_payload;
  //TODO: check signature here
  purpose = 
    GNUNET_malloc (sizeof (struct GNUNET_CRYPTO_EccSignaturePurpose) + 
                   sizeof (struct GNUNET_CRYPTO_EcdhePublicKey) + //E
                   enc_meta_len); // E_K (code_str)
  purpose->size = 
    htonl (sizeof (struct GNUNET_CRYPTO_EccSignaturePurpose) +
           sizeof (struct GNUNET_CRYPTO_EcdhePublicKey) +
           enc_meta_len);
  purpose->purpose = htonl(GNUNET_SIGNATURE_PURPOSE_GNUID_TOKEN_CODE);
  write_ptr = (char*) &purpose[1];
  memcpy (write_ptr, &token_code->ecdh_pubkey, sizeof (struct GNUNET_CRYPTO_EcdhePublicKey));
  write_ptr += sizeof (struct GNUNET_CRYPTO_EcdhePublicKey);
  memcpy (write_ptr, enc_meta, enc_meta_len);

  if (GNUNET_OK != GNUNET_CRYPTO_ecdsa_verify (GNUNET_SIGNATURE_PURPOSE_GNUID_TOKEN_CODE,
                                               purpose,
                                               &token_code->signature,
                                               &token_code_payload->identity_key))
  {
    identity_token_code_destroy (token_code);
    GNUNET_free (token_code_decoded);
    json_decref (root);
    GNUNET_free (purpose);
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Error verifying signature for token code\n");
    return GNUNET_SYSERR;
  }
  *result = token_code;
  GNUNET_free (purpose);

  GNUNET_free (enc_meta);
  GNUNET_free (token_code_decoded);
  json_decref (root);
  return GNUNET_OK;

}



/* end of identity-token.c */