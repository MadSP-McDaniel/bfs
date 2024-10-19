#!/usr/bin/python

import pyjsonrpc

http_client = pyjsonrpc.HttpClient(
    url = "http://localhost:8080/jsonrpc",
    username = "Username",
    password = "Password"
)

print( http_client.call("dev_control", 1) )

# Result: 3

# It is also possible to use the *method* name as *attribute* name.
print( http_client.dev_control(1) )

# Result: 3

# Notifications send messages to the server, without response.
# http_client.notify("add", 3, 4)