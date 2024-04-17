#!/usr/bin/env python3

import socket
import time
import traceback

# https://modern.ircdocs.horse/#connection-registration
# TODO: ping to test buffer sizes

# Binds our test server socket
def bind_socket():
    sock = socket.create_server(("127.0.0.1", 6666))
    print("Created socket")
    return sock

# Closes our test server socket
def unbind_socket(sock):
    sock.close()

# Buffer of received data
recv_buffer = b''

# Fills an empty recieve buffer
def recv_buffer_fill(conn):
    global recv_buffer
    if recv_buffer == b'':
        recv_buffer = conn.recv(512)
        print("BUFFER: %s" % (recv_buffer))
    if recv_buffer == b'':
        raise Exception("Unable to read buffer")

# Reads data out of the receive buffer
# If size is -1, read the entire buffer
def recv_buffer_chomp(length):
    global recv_buffer
    if length == -1:
        buf = recv_buffer
        recv_buffer = b''
        return buf
    else:
        buf = recv_buffer[:length]
        recv_buffer = recv_buffer[length+1:]
        return buf

# Reads a line out of recv_buffer, refilling if needed
# Uses a chunk size of 512, will read a line up to chunk times attempts
# By default that would be 4096
def read_line(conn, attempts=4):
    if attempts == 0:
        raise Exception("Couldn't read line")
    recv_buffer_fill(conn)
    line_feed = recv_buffer.find(b'\n')
    line = recv_buffer_chomp(line_feed)
    if line_feed == -1:
        line += read_line(conn, attempts - 1)
    return line

# Sends a line of text to the client
def send_line(conn, line):
    buf = line.encode('utf-8') + b'\r\n'
    length = conn.send(buf)
    if len(buf) != length:
        raise Exception("Unable to send line")

# Reads an IRC command in to multiple fields
def read_irc_command(conn):
    command = read_line(conn).decode('utf-8')[:-1] # Drop \r
    print("COMMAND: %s " % (command))
    return command.split(' ')

# Requires a login
def require_login(conn):
    # Check we get password first
    password = read_irc_command(conn)
    assert(password[0]=='PASS')
    assert(len(password)==2)
    # Check we get user next
    user = read_irc_command(conn)
    assert(len(user)==5)
    assert(user[0]=='USER')
    assert(user[2] == '0')
    assert(user[3] == '*')
    # Check we get NICK next
    nick = read_irc_command(conn)
    assert(len(nick)==2)
    assert(nick[0]=='NICK')
    # Check all the names are the same
    for name in [user[1], user[4], nick[1]]:
        assert(name == user[1])
    return user[1]

# Sends a fake message of the day
def send_motd(conn, user):
    motds = [
        "001   " + user + "  :Welcome to the test server!",
        "002   " + user + "  :Your host is testserver",
        "003   " + user + "  :Created in 2000",
        "375   " + user + "  :Dear test bot,",
        "372   " + user + "  :I hope you have a lovely day.",
        "376   " + user + "  :Yours, spaghetti man.",
    ]
    for line in motds:
        send_line(conn, ":testserver    " + line)
        #time.sleep(0.01)

# Sends a ping and expects a pong
def test_ping(conn):
    send_line(conn, "PING :testserver hello_world")
    response = read_irc_command(conn)
    assert(response[0]=="PONG")
    assert(response[1]==":testserver")
    assert(response[2]=="hello_world")

# Simulates our fake IRC server
def fake_irc_server(conn):
    print("Testing login...")
    user = require_login(conn)
    print("Sending MOTD...")
    send_motd(conn, user)
    print("Testing ping 1...")
    test_ping(conn)
    print("Testing ping 2...")
    test_ping(conn)
    print("Testing ping 3...")
    test_ping(conn)
    print("All done!")

# Code to run to set up and run single-process IRC
def main():
    sock = bind_socket()
    while True:
        global recv_buffer
        recv_buffer = b''
        try:
            conn, _ = sock.accept()
            fake_irc_server(conn)
            conn.close()
        except Exception as e:
            traceback.print_exc()
            conn.close()
    unbind_socket(sock)

main()
