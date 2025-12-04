# jesenrpc

A C implementation of the [JSON-RPC 2.0](https://www.jsonrpc.org/specification) specification built on top of the [jesen](https://github.com/alwint3r/jesen) JSON library.

## Features

- Full JSON-RPC 2.0 compliance
- Create, serialize, parse, and validate requests and responses
- Support for notifications (requests without IDs)
- Batch request and response handling
- Error object support with standard error codes
- C99 compatible

## Building

```bash
mkdir build && cd build
cmake ..
make
```

To build with tests:

```bash
cmake -DJESENRPC_BUILD_TESTS=ON ..
make
ctest
```

## Installation

```bash
cmake --install . --prefix /usr/local
```

## Usage

### Creating and Serializing a Request

```c
#include "jesenrpc.h"

// Create a request with a numeric ID
jesenrpc_request_t *req = NULL;
jesenrpc_id_t id;
jesenrpc_id_set_number(&id, 1);
jesenrpc_request_create_with_id("subtract", &id, &req);

// Add parameters
jesen_node_t *params = NULL;
jesen_array_create(&params);
jesen_array_add_int32(params, 42);
jesen_array_add_int32(params, 23);
jesenrpc_request_set_params(req, params);

// Serialize to JSON
char buf[256];
jesenrpc_request_serialize(req, buf, sizeof(buf));
// Result: {"jsonrpc":"2.0","id":1,"method":"subtract","params":[42,23]}

jesenrpc_request_destroy(req);
```

### Creating a Notification

```c
// Notifications have no ID and expect no response
jesenrpc_request_t *notif = NULL;
jesenrpc_request_create("update", &notif);

char buf[256];
jesenrpc_request_serialize(notif, buf, sizeof(buf));
// Result: {"jsonrpc":"2.0","method":"update"}

jesenrpc_request_destroy(notif);
```

### Parsing a Response

```c
char json[] = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":19}";
jesenrpc_response_t *resp = NULL;
jesenrpc_response_parse(json, strlen(json), &resp);

if (resp->result) {
    // Handle successful result
}

jesenrpc_response_destroy(resp);
```

### Creating an Error Response

```c
jesenrpc_response_t *resp = NULL;
jesenrpc_response_create(1, &resp);

jesenrpc_error_object_t *error = NULL;
jesenrpc_error_object_create(JESENRPC_JSONRPC_ERROR_METHOD_NOT_FOUND, 
                              "Method not found", &error);
jesenrpc_response_set_error(resp, error);

char buf[256];
jesenrpc_response_serialize(resp, buf, sizeof(buf));

jesenrpc_response_destroy(resp);
```

### Batch Requests

```c
jesenrpc_request_t *requests[2];
// ... create requests ...

char buf[1024];
jesenrpc_request_batch_serialize(requests, 2, buf, sizeof(buf));

// Parsing batch requests
jesenrpc_request_batch_t batch;
jesenrpc_request_batch_parse(json_buf, json_len, &batch);

for (size_t i = 0; i < batch.count; i++) {
    // Process batch.items[i]
}

jesenrpc_request_batch_destroy(&batch);
```

## API Reference

### ID Functions

| Function | Description |
|----------|-------------|
| `jesenrpc_id_set_number()` | Set ID to a numeric value |
| `jesenrpc_id_set_string()` | Set ID to a string value |
| `jesenrpc_id_set_null()` | Set ID to explicit null |
| `jesenrpc_id_set_notification()` | Set ID to none (notification) |
| `jesenrpc_id_destroy()` | Free ID resources |

### Request Functions

| Function | Description |
|----------|-------------|
| `jesenrpc_request_create()` | Create a notification request |
| `jesenrpc_request_create_with_id()` | Create a request with ID |
| `jesenrpc_request_set_id()` | Set the request ID |
| `jesenrpc_request_set_params()` | Set request parameters |
| `jesenrpc_request_is_notification()` | Check if request is a notification |
| `jesenrpc_request_serialize()` | Serialize to JSON string |
| `jesenrpc_request_validate()` | Validate request structure |
| `jesenrpc_request_parse()` | Parse JSON into request |
| `jesenrpc_request_destroy()` | Free request resources |

### Response Functions

| Function | Description |
|----------|-------------|
| `jesenrpc_response_create()` | Create response with numeric ID |
| `jesenrpc_response_create_with_id()` | Create response with ID |
| `jesenrpc_response_create_for_request()` | Create response for a request |
| `jesenrpc_response_set_result()` | Set successful result |
| `jesenrpc_response_set_error()` | Set error object |
| `jesenrpc_response_serialize()` | Serialize to JSON string |
| `jesenrpc_response_validate()` | Validate response structure |
| `jesenrpc_response_parse()` | Parse JSON into response |
| `jesenrpc_response_destroy()` | Free response resources |

### Error Object Functions

| Function | Description |
|----------|-------------|
| `jesenrpc_error_object_create()` | Create error object |
| `jesenrpc_error_object_set_data()` | Set additional error data |
| `jesenrpc_error_object_validate()` | Validate error object |
| `jesenrpc_error_object_destroy()` | Free error object resources |

### Batch Functions

| Function | Description |
|----------|-------------|
| `jesenrpc_request_batch_serialize()` | Serialize request batch |
| `jesenrpc_request_batch_parse()` | Parse request batch |
| `jesenrpc_request_batch_destroy()` | Free request batch |
| `jesenrpc_response_batch_serialize()` | Serialize response batch |
| `jesenrpc_response_batch_parse()` | Parse response batch |
| `jesenrpc_response_batch_destroy()` | Free response batch |

## Standard Error Codes

| Constant | Code | Description |
|----------|------|-------------|
| `JESENRPC_JSONRPC_ERROR_PARSE` | -32700 | Invalid JSON |
| `JESENRPC_JSONRPC_ERROR_INVALID_REQUEST` | -32600 | Invalid request object |
| `JESENRPC_JSONRPC_ERROR_METHOD_NOT_FOUND` | -32601 | Method not found |
| `JESENRPC_JSONRPC_ERROR_INVALID_PARAMS` | -32602 | Invalid parameters |
| `JESENRPC_JSONRPC_ERROR_INTERNAL` | -32603 | Internal error |

Server-defined errors should use codes in the range -32099 to -32000.

