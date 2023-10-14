# c-framework-service
[![GitHub release](https://img.shields.io/github/release/danidomi/c-framework-service.svg)](https://github.com/danidomi/cdeps/releases)

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Installation](#installation)
- [Usage](#usage)
    - [Logging](#logging)
    - [Request](#request)
    - [Response](#response)
- [Contributing](#contributing)
- [License](#license)

## Introduction

The c-framework-service is a C-based framework designed to simplify the development of microservices by providing various features. 
It is suitable for building scalable and efficient microservices that handle web requests and responses.

## Features

The Microservices Framework offers the following features:

- **Logging**: Easily log messages with different log levels (DEBUG, INFO, WARNING, ERROR).
- **Request Handling**: Parse incoming HTTP requests and extract essential information.
- **Response Generation**: Create and customize HTTP responses.

## Getting Started

### Prerequisites

Before using the Microservices Framework, ensure you have the following pre-requeriments:

- C Compiler (gcc)
- Standard C Library (libc)

### Installation

Clone the repository and compile the code to get started:

```shell
cdeps install github.com/danidomi/c-framwework@latest
```

## Usage

### Logging

Use the built-in logging functionality to log messages with various log levels. Example:

```c
#include <c-framework-service/logger/Logger.h>

int main() {
    log_message(DEBUG, "Debug message");
    log_message(INFO, "This is an informational message");
    log_message(ERROR, "An error occurred!");
    return 0;
}
```

### Request

Parse and work with incoming HTTP requests using the Request structure and functions. Example:

```c
#include <c-framework-service/request/Request.h>

int main() {
    char req[] = "GET /api/resource?key=value HTTP/1.1\r\nHost: example.com\r\n\r\n";
    Request *request = parse_request(req);
    char *value = get_query_param_value(request, "key");
    
    // Use the request and extracted query parameters
    // ...
    free(request);
    return 0;
}
```

### Response
Generate and customize HTTP responses with the Response structure. Example:

```c
#include <c-framework-service/response/Response.h>

int main() {
    Response response;
    response.status_code = "200 OK";
    response.headers[0] = "Content-Type: application/json";
    response.data = "{'message': 'Hello, World!'}";

    // Send the response
    // ...
    return 0;
}
```

## Contributing
We welcome contributions from the community. Please see our Contribution Guidelines for details on how to contribute to this project.

## License
This project is licensed under the MIT License. You are free to use and modify it as you see fit.

