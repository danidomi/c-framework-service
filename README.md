# c-framework-service
[![GitHub release](https://img.shields.io/github/release/danidomi/c-framework-service.svg)](https://github.com/danidomi/c-framework-service/releases)

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Installation](#installation)
    - [Configuration](#configuration)
- [Usage](#usage)
    - [Logging](#logging)
    - [Request](#request)
    - [Response](#response)
- [Testing](#testing)
    - [Locally](#locally)
    - [Docker](#docker)
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

Before using the Microservices Framework, ensure you have the following pre-requirements:

- C Compiler (gcc)
- Standard C Library (libc)
- [cdeps](https://github.com/danidomi/cdeps)

### Installation

You can easily set up the Microservices Framework by using the provided dependency manager. Simply run the following command:

```shell
cdeps install github.com/danidomi/c-framwework-service@latest
```


### Configuration

- If you want to change the default PORT, you can do so in the configuration. By default, the PORT is set to 8080.

```c
#ifndef PORT
#define PORT 8080 // Port users will be connecting to
#endif
```

## Usage

### Logging

Use the built-in logging functionality to log messages with various log levels. Example:

```c
#include <c-framework-service/logger/logger.h>

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
#include <c-framework-service/request/request.h>

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
#include <c-framework-service/response/response.h>

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

## Testing

You can use the `curl` command to test the "hello" endpoint and ensure that your c-framework-service is working correctly.

Open a browser or a new terminal window make a GET request to the "hello" endpoint.
```shell
curl http://localhost:8080/hello
```

### Locally

Make sure your Microservices Framework is up and running. If you haven't started it yet, run the following command to start it on the default port (8080):

```shell
./run.sh
```

### Docker

Build the image
```shell
docker build -t c-framework-service .
```

and run it.

```shell
docker run -p 8080:8080 c-framework-service
```

## Contributing
We welcome contributions from the community. Please see our Contribution Guidelines for details on how to contribute to this project.

## License
This project is licensed under the MIT License. You are free to use and modify it as you see fit.

