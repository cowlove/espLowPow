#!/usr/bin/python3 -u 

import socket, os, sys, re, datetime, hashlib 

HOST = "0"  # Standard loopback interface address (localhost)
PORT = int(sys.argv[1])  # Port to listen on (non-privileged ports are > 1023)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((HOST, PORT))
print(f"Listening on port {PORT}")
s.listen()
while True:
    conn, addr = s.accept()
    if (os.fork() == 0):
        try:
            #print("Got connection")
            conn.settimeout(3.0)
            data = conn.recv(1024).decode()
            #print("Got data")
            if data and re.match('.*{"Tie', data):
                print(datetime.datetime.now().isoformat() + " " + data.rstrip())
                hash = hashlib.md5(data.encode())
                conn.sendall((hash.hexdigest() + "\n").encode())
        except Exception as e:
            print(e)
        try:
            conn.shutdown(socket.SHUT_RDWR)
            conn.close() 
        except:
            0
        break

