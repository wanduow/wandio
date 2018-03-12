/*
 *
 * Copyright (c) 2007-2016 The University of Waikato, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This file is part of libwandio.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libwandio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * libwandio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/* Author: Alistair King <alistair@caida.org> */

#include "config.h"
#include "wandio.h"
#include "curl-helper.h"
#include "swift-keystone.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper for Swift module that does Swift Keystone V3 Auth */

#define DEBUG_CURL 1L

#define BUFLEN 1024
#define AUTH_REQ_TMPL                             \
  "{"                                             \
  "  \"auth\": {"                                 \
  "    \"identity\": {"                          \
  "      \"methods\": [\"password\"],"           \
  "      \"password\": {"                        \
  "        \"user\": {"                          \
  "          \"name\": \"%s\","                  \
  "          \"domain\": { \"id\": \"%s\" },"    \
  "          \"password\" \"%s\""                \
  "        }"                                    \
  "      }"                                      \
  "    }"                                        \
  "  }"                                          \
  "}"

static int build_auth_request_payload(char *buf, size_t buf_len,
                                      keystone_auth_creds_t *creds)
{
  size_t written;
  if ((written = snprintf(buf, buf_len, AUTH_REQ_TMPL,
                          creds->username, creds->domain_id,
                          creds->password)) >= buf_len) {
    return -1;
  }

  return written + 1;
}

#define GETENV(env, dst, req)                                   \
  do {                                                          \
    char *tmp;                                                  \
    if ((tmp = getenv(env)) == NULL) {                          \
      if (req) success = 0;                                     \
    } else if (((dst) = strdup(tmp)) == NULL) {                 \
      return -1;                                                \
    }                                                           \
  } while (0)

int keystone_env_parse_creds(keystone_auth_creds_t *creds)
{
  int success = 1;

  GETENV("OS_AUTH_URL", creds->auth_url, 1);
  GETENV("OS_USERNAME", creds->username, 1);
  GETENV("OS_PASSWORD", creds->password, 1);
  GETENV("OS_PROJECT_NAME", creds->project, 1);
  GETENV("OS_PROJECT_DOMAIN_ID", creds->domain_id, 0);

  return success;
}

int keystone_env_parse_token(keystone_auth_token_t *token)
{
  int success = 1;

  GETENV("OS_AUTH_TOKEN", token->token, 1);
  GETENV("OS_STORAGE_URL", token->storage_url, 1);

  return success;
}

void keystone_auth_creds_destroy(keystone_auth_creds_t *creds)
{
  free(creds->auth_url);
  free(creds->username);
  free(creds->password);
  free(creds->project);
  free(creds->domain_id);
}

void keystone_auth_token_destroy(keystone_auth_token_t *token)
{
  free(token->token);
  free(token->storage_url);
}

int keystone_authenticate(keystone_auth_creds_t *creds,
                          keystone_auth_token_t *auth)
{
  CURL *ch = NULL;
  struct curl_slist *headers = NULL;
  char buf[BUFLEN];

  memset(auth, 0, sizeof(*auth));

  curl_helper_safe_global_init();

  if ((ch = curl_easy_init()) == NULL) {
    goto err;
  }

  // set options
  if (curl_easy_setopt(ch, CURLOPT_VERBOSE, DEBUG_CURL) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_URL, creds->auth_url) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POST, 1L) != CURLE_OK) {
    goto err;
  }

  headers = curl_slist_append(headers, "Content-Type: application/json");

  if (build_auth_request_payload(buf, sizeof(buf), creds) != 0) {
    goto err;
  }

  // TODO: here

  return 0;

 err:
  curl_slist_free_all(headers);
  return -1;
}

void keystone_auth_token_dump(keystone_auth_token_t *token)
{
  if (token == NULL || token->token == NULL || token->storage_url == NULL) {
    return;
  }
  printf("export OS_STORAGE_URL=%s\n", token->storage_url);
  printf("export OS_AUTH_TOKEN=%s\n", token->token);
}
