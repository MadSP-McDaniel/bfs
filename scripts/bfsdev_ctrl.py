#!/usr/bin/python

import pyjsonrpc

# Build the RPC request handler 
class RequestHandler(pyjsonrpc.HttpRequestHandler):

    # Run a command on the device
    @pyjsonrpc.rpcmethod
    def dev_control(self, a):
        return( "text ")

# Threading HTTP-Server
http_server = pyjsonrpc.ThreadingHttpServer(
    server_address = ('localhost', 8080),
    RequestHandlerClass = RequestHandler
)

print( "Starting HTTP server ..." )
print( "URL: http://localhost:8080" )

# Serve the requests forever
http_server.serve_forever()
