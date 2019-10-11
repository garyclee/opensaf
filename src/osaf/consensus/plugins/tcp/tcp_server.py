#!/usr/bin/env python3
"""
     -*- OpenSAF  -*-

(C) Copyright 2019 Ericsson AB 2019 - All Rights Reserved.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
under the GNU Lesser General Public License Version 2.1, February 1999.
The complete license can be accessed from the following location:
http://opensource.org/licenses/lgpl-license.php
See the Copying file included with the OpenSAF distribution for full
licensing terms.

Simple TCP arbitrator
"""

import os
import socket
from base64 import b64decode
from socketserver import ThreadingMixIn
from xmlrpc.server import SimpleXMLRPCServer, SimpleXMLRPCRequestHandler
import ssl
import sys
import threading
import time

# Warning: example only, replace with signed certificate
# Assumes pem files are located in the same directory as this file
DIRPATH = os.path.dirname(os.path.realpath(__file__))
CERTFILE = DIRPATH + '/certificate.pem'
KEYFILE = DIRPATH + '/key.pem'
USERNAME = 'test'
PASSWORD = 'test'


class RequestHandler(SimpleXMLRPCRequestHandler):
    """ Allow keep alives """
    protocol_version = "HTTP/1.1"

    def parse_request(self):
        if SimpleXMLRPCRequestHandler.parse_request(self):
            if self.auth(self.headers):
                return True
            else:
                self.send_error(401, 'Unauthorized')
        return False

    def auth(self, headers):
        (basic, _, encoded) = headers.get('Authorization').partition(' ')
        assert basic == 'Basic', 'Only basic authentication supported'
        decoded_bytes = b64decode(encoded)
        decoded_string = decoded_bytes.decode()
        (username, _, password) = decoded_string.partition(':')
        if username == USERNAME and password == PASSWORD:
            return True
        return False


class ThreadedRPCServer(ThreadingMixIn,
                        SimpleXMLRPCServer):
    """ Add thread support """
    def __init__(self, bind_address, bind_port,
                 requestHandler):
        # IPv6 is supported
        self.address_family = socket.getaddrinfo(bind_address, bind_port)[0][0]
        SimpleXMLRPCServer.__init__(self, (bind_address, bind_port),
                                    requestHandler, logRequests=False)
        self.socket = ssl.wrap_socket(
            socket.socket(self.address_family, self.socket_type),
            server_side=True,
            certfile=CERTFILE,
            keyfile=KEYFILE,
            cert_reqs=ssl.CERT_NONE,
            ssl_version=ssl.PROTOCOL_TLSv1_2,
            do_handshake_on_connect=False)
        self.server_bind()
        self.server_activate()

    def finish_request(self, request, client_address):
         request.do_handshake()
         return SimpleXMLRPCServer.finish_request(self, request, client_address)


class Arbitrator(object):
    """ Implementation of a simple arbitrator """

    def __init__(self):
        self.port = 6666
        self.kv_pairs = {}
        self.mutex = threading.Lock()

    def heartbeat(self, key):
        timestamp = int(time.time())
        self.mutex.acquire()
        self.kv_pairs[key] = timestamp
        self.mutex.release()
        return timestamp

    def set(self, key, value):
        self.mutex.acquire()
        self.kv_pairs[key] = value
        self.mutex.release()
        return True

    def get(self, key):
        value = ""
        self.mutex.acquire()
        if key in self.kv_pairs:
            value = self.kv_pairs[key]
        self.mutex.release()
        return value

    def set_if_prev(self, key, prev_value, new_value):
        result = False
        self.mutex.acquire()
        if key in self.kv_pairs:
            value = self.kv_pairs[key]
            if value == prev_value:
                self.kv_pairs[key] = new_value
                result = True
        self.mutex.release()
        return result

    def create(self, key, value):
        result = False
        self.mutex.acquire()
        if key not in self.kv_pairs:
            self.kv_pairs[key] = value
            result = True
        self.mutex.release()
        return result

    def delete(self, key):
        result = False
        self.mutex.acquire()
        if key in self.kv_pairs:
            del self.kv_pairs[key]
            result = True
        self.mutex.release()
        return result

    def run(self):
        hostname = "::0.0.0.0"
        server = ThreadedRPCServer(hostname, self.port,
                                   requestHandler=RequestHandler)
        print("Listening on port %d" % self.port)
        server.register_multicall_functions()
        server.register_function(self.heartbeat, 'heartbeat')
        server.register_function(self.set, 'set')
        server.register_function(self.get, 'get')
        server.register_function(self.set_if_prev, 'set_if_prev')
        server.register_function(self.create, 'create')
        server.register_function(self.delete, 'delete')
        server.serve_forever()

        sys.exit(0)


if __name__ == '__main__':
    Arbitrator().run()
