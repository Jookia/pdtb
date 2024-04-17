#!/usr/bin/env python3

import subprocess
import time
import signal
import os
import socket
import select
import random

# Binds our test server socket
def bind_socket():
    sock = socket.create_server(("127.0.0.1", 6666))
    print("Created socket")
    return sock

# Accepts a connection
def accept_socket(sock):
    conn, _ = sock.accept()
    return conn

# Closes our test server socket
def unbind_socket(sock):
    sock.close()

# Gets the next packet from a connection, timing out if needed
def get_next_packet(conn, timeout):
    (rlist, wlist, xlist) = select.select([conn], [], [], timeout)
    if rlist == [conn]:
        packet = conn.recv(5000)
        return packet
    else:
        return None

# Launches the test DOSBox using Flatpak
# Returns an instance ID for controlling the process
def launch_dosbox():
    (pread, pwrite) = os.pipe()
    os.set_inheritable(pwrite, True)
    print("Launching test DOSBox!")
    #os.environ["SDL_VIDEODRIVER"] = "dummy"
    proc = subprocess.Popen(["flatpak", "run", "--instance-id-fd=" + str(pwrite), "com.dosbox_x.DOSBox-X", "-conf", "dosbox-x.test.conf"], pass_fds=[pwrite], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    instance = os.read(pread, 512)
    os.close(pread)
    print("Got instance %s" % (instance))
    return instance

# Kills a DOSBox Flatpak instance
def kill_dosbox(instance):
    proc = subprocess.Popen(["flatpak", "kill", instance])

# Reads the packet payload as a number
def read_packet_number(status):
    if len(status) != 32:
        print("status != 32")
    num = int(status.split(b" ")[4].split(b"\n")[0].decode("utf-8"))
    return num

# Check these particular sizes to confirm that packet size works
# We could use more but these hit the general notes
test_sizes = [1, 10, 20, 100, 200, 300, 400, 410, 418, 419, 420, 421, 450, 460, 500, 600, 839, 840, 841, 1500, 1678, 1679, 1680, 1681, 2000, 3000, 4000, 5000]
# Shuffle them for good fortune
random.shuffle(test_sizes)

# Test that the bot can take a certain amount of data
def run_input_test(conn):
    print("Running input test")
    for i in test_sizes:
        # Send our long packet
        conn.send(b"A" * i)
        # See what we get back
        status = get_next_packet(conn, 5.0)
        if status:
            # Get the length value contained in the packet
            nums = [read_packet_number(status)]
            next_packet = get_next_packet(conn, 0.1)
            while next_packet != None:
                # Do the same for any remaining packets
                nums += [read_packet_number(next_packet)]
                next_packet = get_next_packet(conn, 0.1)
            # Add the sums up to check no packets were mangled or dropped
            summ = sum(nums)
            if summ != i:
                print("Expected %i total, got %s (sum %i)" % (i, nums, summ))
                return 0
        else:
            print("Timed out!")
            return 0

# Test that the bot can send a certain amount of data
def run_output_test(conn):
    print("Running output test")
    for i in test_sizes:
        # Send a request for bytes
        print("Asking for %i bytes" % (i))
        conn.send(b"send me %i please\0" % (i))
        status = get_next_packet(conn, 5.0)
        if status:
            # Count the bytes in the packet
            nums = [len(status)]
            next_packet = get_next_packet(conn, 0.1)
            while next_packet != None:
                # Do the same for any remaining packets
                nums += [len(next_packet)]
                next_packet = get_next_packet(conn, 0.1)
            # Add the sums together to make sure we have all the packets
            summ = sum(nums)
            if summ != i:
                print("Expected %i total, got %s (sum %i)" % (i, nums, summ))
                return 0
        else:
            print("Timed out!")
            return 0

# Code to run to set up and run tests
def main():
    sock = bind_socket()
    try:
        flatpak = launch_dosbox()
        conn = accept_socket(sock)
        run_output_test(conn)
        conn.close()
        conn = accept_socket(sock)
        run_input_test(conn)
        conn.close()
    except KeyboardInterrupt:
        pass
    kill_dosbox(flatpak)
    unbind_socket(sock)

main()
