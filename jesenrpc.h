/**
 * @file jesenrpc.h
 * @brief JSON-RPC 2.0 implementation built on top of the jesen JSON library.
 *
 * This library provides a C implementation of the JSON-RPC 2.0 specification.
 * It supports creating, serializing, parsing, and validating JSON-RPC requests,
 * responses, and error objects. Batch requests and responses are also
 * supported.
 *
 * @see https://www.jsonrpc.org/specification
 *
 * ## Usage Example
 *
 * ### Creating and serializing a request:
 * @code
 * jesenrpc_request_t *req = NULL;
 * jesenrpc_id_t id;
 * jesenrpc_id_set_number(&id, 1);
 * jesenrpc_request_create_with_id("subtract", &id, &req);
 *
 * jesen_node_t *params = NULL;
 * jesen_array_create(&params);
 * jesen_array_add_int32(params, 42);
 * jesen_array_add_int32(params, 23);
 * jesenrpc_request_set_params(req, params);
 *
 * char buf[256];
 * jesenrpc_request_serialize(req, buf, sizeof(buf));
 * // buf: {"jsonrpc":"2.0","id":1,"method":"subtract","params":[42,23]}
 *
 * jesenrpc_request_destroy(req);
 * @endcode
 *
 * ### Parsing a response:
 * @code
 * char json[] = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":19}";
 * jesenrpc_response_t *resp = NULL;
 * jesenrpc_response_parse(json, strlen(json), &resp);
 *
 * if (resp->result) {
 *     // Handle successful result
 * }
 * jesenrpc_response_destroy(resp);
 * @endcode
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jesen.h"

// clang-format off
#ifndef JESENRPC_API
	#if defined(_WIN32) || defined(__CYGWIN__)
		#if defined(JESENRPC_SHARED)
			#if defined(JESENRPC_BUILDING_SHARED)
				#define JESENRPC_API __declspec(dllexport)
			#else
				#define JESENRPC_API __declspec(dllimport)
			#endif
		#else
			#define JESENRPC_API
		#endif
	#else
		#if defined(JESENRPC_SHARED) && __GNUC__ >= 4
			#define JESENRPC_API __attribute__((visibility("default")))
		#else
			#define JESENRPC_API
		#endif
	#endif
#endif
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup constants Constants and Limits
 * @{
 */

/** Maximum length for a JSON-RPC method name. */
#define JESENRPC_METHOD_NAME_MAX_LEN 256

/** JSON-RPC protocol version string. */
#define JESENRPC_JSONRPC_VERSION "2.0"

/** Length of the JSON-RPC version string (excluding null terminator). */
#define JESENRPC_JSONRPC_VERSION_LEN 3

/** @} */

/**
 * @defgroup errors Error Codes
 * @{
 */

/** Error type used by all jesenrpc functions. */
typedef int32_t jesenrpc_err_t;

/** No error occurred. */
#define JESENRPC_ERR_NONE 0

/** Base value for jesenrpc-specific error codes. */
#define JESENRPC_ERR_BASE JESEN_ERR_BASE + 0xFF

/** Invalid arguments passed to a function. */
#define JESENRPC_ERR_INVALID_ARGS (JESENRPC_ERR_BASE + 1)

/** Validation failed (e.g., missing required fields, invalid format). */
#define JESENRPC_ERR_VALIDATION (JESENRPC_ERR_BASE + 2)

/** Memory allocation failed. */
#define JESENRPC_ERR_ALLOC (JESENRPC_ERR_BASE + 3)

/** @} */

/**
 * @defgroup jsonrpc_errors JSON-RPC Standard Error Codes
 * @brief Pre-defined error codes from the JSON-RPC 2.0 specification.
 * @{
 */

/** Invalid JSON was received by the server. */
#define JESENRPC_JSONRPC_ERROR_PARSE -32700

/** The JSON sent is not a valid Request object. */
#define JESENRPC_JSONRPC_ERROR_INVALID_REQUEST -32600

/** The method does not exist or is not available. */
#define JESENRPC_JSONRPC_ERROR_METHOD_NOT_FOUND -32601

/** Invalid method parameter(s). */
#define JESENRPC_JSONRPC_ERROR_INVALID_PARAMS -32602

/** Internal JSON-RPC error. */
#define JESENRPC_JSONRPC_ERROR_INTERNAL -32603

/** Start of server-defined error code range (inclusive). */
#define JESENRPC_JSONRPC_ERROR_SERVER_MIN -32099

/** End of server-defined error code range (inclusive). */
#define JESENRPC_JSONRPC_ERROR_SERVER_MAX -32000

/** @} */

/**
 * @defgroup types Data Types
 * @{
 */

/**
 * @brief Represents the type of a JSON-RPC request/response ID.
 *
 * Per the JSON-RPC 2.0 spec, an ID can be a number, string, or null.
 * JESENRPC_ID_NONE indicates a notification (no ID field present).
 */
typedef enum jesenrpc_id_kind {
  JESENRPC_ID_NONE = 0, /**< Notification (id field absent). */
  JESENRPC_ID_NUMBER,   /**< Integer ID. */
  JESENRPC_ID_STRING,   /**< String ID. */
  JESENRPC_ID_NULL      /**< Explicit null ID. */
} jesenrpc_id_kind_t;

/**
 * @brief Represents a JSON-RPC request/response ID.
 *
 * Use the jesenrpc_id_set_* functions to populate this structure.
 */
typedef struct jesenrpc_id {
  jesenrpc_id_kind_t kind; /**< The type of ID value. */
  union {
    int64_t number; /**< Integer ID value (when kind == JESENRPC_ID_NUMBER). */
    struct {
      char *data; /**< String ID data (when kind == JESENRPC_ID_STRING). */
      size_t len; /**< Length of string ID. */
    } string;
  } value;
} jesenrpc_id_t;

/**
 * @brief Represents a JSON-RPC error object.
 *
 * Contains an error code, human-readable message, and optional data.
 * Use jesenrpc_error_object_create() to allocate and initialize.
 */
typedef struct jesenrpc_error_object {
  int32_t code;  /**< Error code (use JESENRPC_JSONRPC_ERROR_* constants). */
  char *message; /**< Human-readable error description. */
  jesen_node_t *data; /**< Optional additional error data. May be NULL. */
} jesenrpc_error_object_t;

/**
 * @brief Represents a JSON-RPC request object.
 *
 * A request without an ID (id.kind == JESENRPC_ID_NONE) is a notification.
 * Use jesenrpc_request_create() or jesenrpc_request_create_with_id() to
 * allocate.
 */
typedef struct jesenrpc_request {
  const char *jsonrpc; /**< Protocol version, always "2.0". */
  jesenrpc_id_t id;    /**< Request ID. JESENRPC_ID_NONE for notifications. */
  char *method_name;   /**< Method name to invoke. */
  jesen_node_t
      *params; /**< Method parameters. May be NULL. Must be array or object. */
} jesenrpc_request_t;

/**
 * @brief Represents a JSON-RPC response object.
 *
 * A response contains either a result OR an error, never both.
 * Use jesenrpc_response_create() or jesenrpc_response_create_for_request() to
 * allocate.
 */
typedef struct jesenrpc_response {
  const char *jsonrpc;            /**< Protocol version, always "2.0". */
  jesenrpc_id_t id;               /**< Response ID (must match request ID). */
  jesen_node_t *result;           /**< Result value. NULL when error is set. */
  jesenrpc_error_object_t *error; /**< Error object. NULL when result is set. */
} jesenrpc_response_t;

/**
 * @brief Container for a batch of JSON-RPC requests.
 *
 * Used when parsing or serializing multiple requests as a JSON array.
 */
typedef struct jesenrpc_request_batch {
  jesenrpc_request_t **items; /**< Array of request pointers. */
  size_t count;               /**< Number of requests in the batch. */
} jesenrpc_request_batch_t;

/**
 * @brief Container for a batch of JSON-RPC responses.
 *
 * Used when parsing or serializing multiple responses as a JSON array.
 */
typedef struct jesenrpc_response_batch {
  jesenrpc_response_t **items; /**< Array of response pointers. */
  size_t count;                /**< Number of responses in the batch. */
} jesenrpc_response_batch_t;

/** @} */

/**
 * @defgroup id_functions ID Functions
 * @brief Functions for manipulating JSON-RPC ID values.
 * @{
 */

/**
 * @brief Sets an ID to a numeric value.
 * @param id Pointer to the ID structure to modify.
 * @param value The integer value to set.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_id_set_number(jesenrpc_id_t *id,
                                                   int64_t value);

/**
 * @brief Sets an ID to a string value.
 * @param id Pointer to the ID structure to modify.
 * @param value The string value (copied internally).
 * @param value_len Length of the string.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_id_set_string(jesenrpc_id_t *id,
                                                   const char *value,
                                                   size_t value_len);

/**
 * @brief Sets an ID to explicit null.
 * @param id Pointer to the ID structure to modify.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_id_set_null(jesenrpc_id_t *id);

/**
 * @brief Sets an ID to indicate a notification (no ID field).
 * @param id Pointer to the ID structure to modify.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_id_set_notification(jesenrpc_id_t *id);

/**
 * @brief Frees resources associated with an ID.
 * @param id Pointer to the ID structure to clean up.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note Only needed for string IDs; safe to call on any ID type.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_id_destroy(jesenrpc_id_t *id);

/** @} */

/**
 * @defgroup request_functions Request Functions
 * @brief Functions for creating, manipulating, and serializing JSON-RPC
 * requests.
 * @{
 */

/**
 * @brief Creates a new JSON-RPC request (notification, no ID).
 * @param method_name The method name to invoke (max
 * JESENRPC_METHOD_NAME_MAX_LEN chars).
 * @param request Output pointer to receive the allocated request.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note The returned request is a notification. Use jesenrpc_request_set_id()
 *       or jesenrpc_request_create_with_id() for requests expecting a response.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_request_create(const char *method_name, jesenrpc_request_t **request);

/**
 * @brief Creates a new JSON-RPC request with a specific ID.
 * @param method_name The method name to invoke.
 * @param id The request ID (copied internally).
 * @param request Output pointer to receive the allocated request.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_request_create_with_id(
    const char *method_name, const jesenrpc_id_t *id,
    jesenrpc_request_t **request);

/**
 * @brief Sets the ID of an existing request.
 * @param request The request to modify.
 * @param id The ID to set (copied internally).
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_request_set_id(jesenrpc_request_t *request,
                                                    const jesenrpc_id_t *id);

/**
 * @brief Sets the parameters for a request.
 * @param request The request to modify.
 * @param params The parameters (must be a jesen array or object). Ownership
 * transferred.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note The request takes ownership of params. Do not free params separately.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_request_set_params(jesenrpc_request_t *request, jesen_node_t *params);

/**
 * @brief Checks if a request is a notification.
 * @param request The request to check.
 * @return true if the request has no ID (is a notification), false otherwise.
 */
JESENRPC_API bool
jesenrpc_request_is_notification(const jesenrpc_request_t *request);

/**
 * @brief Serializes a request to a JSON string.
 * @param request The request to serialize.
 * @param out_buf Buffer to write the JSON string.
 * @param out_buf_len Size of the output buffer.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_request_serialize(
    const jesenrpc_request_t *request, char *out_buf, size_t out_buf_len);

/**
 * @brief Validates a request structure.
 * @param request The request to validate.
 * @return JESENRPC_ERR_NONE if valid, or an error code describing the issue.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_request_validate(const jesenrpc_request_t *request);

/**
 * @brief Parses a JSON string into a request structure.
 * @param buf The JSON string buffer (may be modified during parsing).
 * @param buf_len Length of the JSON string.
 * @param out Output pointer to receive the parsed request.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note Caller is responsible for calling jesenrpc_request_destroy() on the
 * result.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_request_parse(char *buf, size_t buf_len,
                                                   jesenrpc_request_t **out);

/**
 * @brief Frees a request and all associated resources.
 * @param request The request to destroy.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_request_destroy(jesenrpc_request_t *request);

/** @} */

/**
 * @defgroup response_functions Response Functions
 * @brief Functions for creating, manipulating, and serializing JSON-RPC
 * responses.
 * @{
 */

/**
 * @brief Creates a response for a given request, copying its ID.
 * @param request The request to respond to (must have an ID, not a
 * notification).
 * @param response Output pointer to receive the allocated response.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_create_for_request(
    const jesenrpc_request_t *request, jesenrpc_response_t **response);

/**
 * @brief Creates a response with a specific ID.
 * @param id The response ID (copied internally).
 * @param response Output pointer to receive the allocated response.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_create_with_id(
    const jesenrpc_id_t *id, jesenrpc_response_t **response);

/**
 * @brief Creates a response with a numeric ID.
 * @param request_id The numeric ID for the response.
 * @param response Output pointer to receive the allocated response.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_response_create(int32_t request_id, jesenrpc_response_t **response);

/**
 * @brief Sets the result value for a successful response.
 * @param response The response to modify.
 * @param result The result value. Ownership transferred.
 * @return JESENRPC_ERR_NONE on success, or JESENRPC_ERR_INVALID_ARGS if
 *         result/error is already set.
 * @note Cannot be called if an error is already set.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_set_result(
    jesenrpc_response_t *response, jesen_node_t *result);

/**
 * @brief Sets the error object for a failed response.
 * @param response The response to modify.
 * @param error The error object. Ownership transferred.
 * @return JESENRPC_ERR_NONE on success, or JESENRPC_ERR_INVALID_ARGS if
 *         result/error is already set.
 * @note Cannot be called if a result is already set.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_set_error(
    jesenrpc_response_t *response, jesenrpc_error_object_t *error);

/**
 * @brief Serializes a response to a JSON string.
 * @param response The response to serialize.
 * @param out_buf Buffer to write the JSON string.
 * @param out_buf_len Size of the output buffer.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_serialize(
    const jesenrpc_response_t *response, char *out_buf, size_t out_buf_len);

/**
 * @brief Validates a response structure.
 * @param response The response to validate.
 * @return JESENRPC_ERR_NONE if valid, or an error code describing the issue.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_response_validate(const jesenrpc_response_t *response);

/**
 * @brief Parses a JSON string into a response structure.
 * @param buf The JSON string buffer (may be modified during parsing).
 * @param buf_len Length of the JSON string.
 * @param out Output pointer to receive the parsed response.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note Caller is responsible for calling jesenrpc_response_destroy() on the
 * result.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_parse(char *buf, size_t buf_len,
                                                    jesenrpc_response_t **out);

/**
 * @brief Frees a response and all associated resources.
 * @param response The response to destroy.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_response_destroy(jesenrpc_response_t *response);

/** @} */

/**
 * @defgroup error_functions Error Object Functions
 * @brief Functions for creating and manipulating JSON-RPC error objects.
 * @{
 */

/**
 * @brief Creates a new error object.
 * @param code The error code (see JESENRPC_JSONRPC_ERROR_* constants).
 * @param message Human-readable error description (copied internally).
 * @param error Output pointer to receive the allocated error object.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_error_object_create(
    int32_t code, const char *message, jesenrpc_error_object_t **error);

/**
 * @brief Sets additional data on an error object.
 * @param error The error object to modify.
 * @param data Additional error data. Ownership transferred. May be NULL to
 * clear.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_error_object_set_data(
    jesenrpc_error_object_t *error, jesen_node_t *data);

/**
 * @brief Validates an error object structure.
 * @param error The error object to validate.
 * @return JESENRPC_ERR_NONE if valid, or an error code describing the issue.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_error_object_validate(const jesenrpc_error_object_t *error);

/**
 * @brief Frees an error object and all associated resources.
 * @param error The error object to destroy.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_error_object_destroy(jesenrpc_error_object_t *error);

/** @} */

/**
 * @defgroup batch_functions Batch Functions
 * @brief Functions for handling JSON-RPC batch requests and responses.
 * @{
 */

/**
 * @brief Serializes multiple requests as a JSON array.
 * @param requests Array of request pointers to serialize.
 * @param request_count Number of requests in the array.
 * @param out_buf Buffer to write the JSON array string.
 * @param out_buf_len Size of the output buffer.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_request_batch_serialize(
    jesenrpc_request_t *const *requests, size_t request_count, char *out_buf,
    size_t out_buf_len);

/**
 * @brief Serializes multiple responses as a JSON array.
 * @param responses Array of response pointers to serialize.
 * @param response_count Number of responses in the array.
 * @param out_buf Buffer to write the JSON array string.
 * @param out_buf_len Size of the output buffer.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_batch_serialize(
    jesenrpc_response_t *const *responses, size_t response_count, char *out_buf,
    size_t out_buf_len);

/**
 * @brief Parses a JSON array into a batch of requests.
 * @param buf The JSON array string (may be modified during parsing).
 * @param buf_len Length of the JSON string.
 * @param out Output structure to receive the parsed requests.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note Caller is responsible for calling jesenrpc_request_batch_destroy() on
 * the result.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_request_batch_parse(
    char *buf, size_t buf_len, jesenrpc_request_batch_t *out);

/**
 * @brief Parses a JSON array into a batch of responses.
 * @param buf The JSON array string (may be modified during parsing).
 * @param buf_len Length of the JSON string.
 * @param out Output structure to receive the parsed responses.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 * @note Caller is responsible for calling jesenrpc_response_batch_destroy() on
 * the result.
 */
JESENRPC_API jesenrpc_err_t jesenrpc_response_batch_parse(
    char *buf, size_t buf_len, jesenrpc_response_batch_t *out);

/**
 * @brief Frees a request batch and all contained requests.
 * @param batch The batch to destroy.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_request_batch_destroy(jesenrpc_request_batch_t *batch);

/**
 * @brief Frees a response batch and all contained responses.
 * @param batch The batch to destroy.
 * @return JESENRPC_ERR_NONE on success, or an error code.
 */
JESENRPC_API jesenrpc_err_t
jesenrpc_response_batch_destroy(jesenrpc_response_batch_t *batch);

/** @} */

#ifdef __cplusplus
}
#endif
