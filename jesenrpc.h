#pragma once

#include <stdint.h>

#include "jesen.h"

#define JESENRPC_METHOD_NAME_MAX_LEN 256

typedef int32_t jesenrpc_err_t;

#define JESENRPC_ERR_NONE 0
#define JESENRPC_ERR_BASE JESEN_ERR_BASE + 0xFF

typedef struct jesenrpc_request {
  int32_t id;
  char *method_name;
  jesen_node_t *params;
} jesenrpc_request_t;

typedef struct jesenrpc_response {
  int32_t id;
  jesen_node_t *result;
  jesen_node_t *error;
} jesenrpc_response_t;

jesenrpc_err_t jesenrpc_create_request(const char *method_name,
                                       jesenrpc_request_t **request);

// BEGIN optional APIs
jesenrpc_err_t jesenrpc_request_set_id(jesenrpc_request_t *request, int32_t id);
jesenrpc_err_t jesenrpc_request_set_params(jesenrpc_request_t *request,
                                           jesen_node_t *params);
// END optional APIs
jesenrpc_err_t jesenrpc_request_serialize(const jesenrpc_request_t *request,
                                          char *out_buf, size_t out_buf_len);
jesenrpc_err_t jesenrcp_request_validate(const jesenrpc_request_t *request);

jesenrpc_err_t jesenrpc_request_parse(char *buf, size_t buf_len,
                                      jesenrpc_request_t **out);
jesenrpc_err_t jesenrpc_request_destroy(jesenrpc_request_t *request);

jesenrpc_err_t jesenrpc_response_create(int request_id,
                                        jesenrpc_response_t **response);
jesenrpc_err_t jesenrpc_response_serialize(const jesenrpc_response_t *response,
                                           char *out_buf, size_t out_buf_len);
jesenrpc_err_t jesenrpc_response_validate(const jesenrpc_response_t *response);
jesenrpc_err_t jesenrpc_response_parse(char *buf, size_t buf_len,
                                       jesenrpc_response_t **out);
jesenrpc_err_t jesenrpc_response_destroy(jesenrpc_response_t *response);
