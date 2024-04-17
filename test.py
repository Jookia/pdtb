#!/usr/bin/env python3

import subprocess
import time
import signal
import os
import socket
import select
import random

def bind_socket():
    sock = socket.create_server(("127.0.0.1", 6666))
    print("Created socket")
    return sock

def accept_socket(sock):
    conn, _ = sock.accept()
    return conn

def unbind_socket(sock):
    sock.close()

def launch_dosbox():
    (pread, pwrite) = os.pipe()
    os.set_inheritable(pwrite, True)
    print("Launching test DOSBox!")
    #os.environ["SDL_VIDEODRIVER"] = "dummy"
    proc = subprocess.Popen(["flatpak", "run", "--instance-id-fd=" + str(pwrite), "com.dosbox_x.DOSBox-X", "-conf", "/home/jookia/Desktop/bot/dosbox-x.test.conf"], pass_fds=[pwrite], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    instance = os.read(pread, 512)
    os.close(pread)
    print("Got instance %s" % (instance))
    return instance

def kill_dosbox(instance):
    proc = subprocess.Popen(["flatpak", "kill", instance])

max_input_size = 420
all_tests = [1, 10, 20, 100, 200, 300, 400, 410, 418, 419, 420, 421, 450, 460, 500, 600, 839, 840, 841, 1500, 1678, 1679, 1680, 1681, 2000, 3000, 4000, 5000]

def get_next_packet(conn, timeout):
    (rlist, wlist, xlist) = select.select([conn], [], [], timeout)
    if rlist == [conn]:
        packet = conn.recv(5000)
        return packet
    else:
        return None

def read_packet_number(status):
    if len(status) != 32:
        print("status != 32")
    num = int(status.split(b" ")[4].split(b"\n")[0].decode("utf-8"))
    return num

def run_tests(conn):
    for i in all_tests:
        print("Trying %i bytes" %(i))
        conn.send(b"A" * i)
        status = get_next_packet(conn, 5.0)
        if status:
            nums = [read_packet_number(status)]
            next_packet = get_next_packet(conn, 0.1)
            while next_packet != None:
                nums += [read_packet_number(next_packet)]
                next_packet = get_next_packet(conn, 0.1)
            print(nums)
            summ = sum(nums)
            if summ != i:
                print("Expected %i total, got %s (sum %i)" % (i, nums, summ))
                #return 0
        else:
            print("Timed out!")
            #return 0

def main():
    sock = bind_socket()
    try:
        flatpak = launch_dosbox()
        conn = accept_socket(sock)
        run_tests(conn)
    except KeyboardInterrupt:
        pass
    kill_dosbox(flatpak)
    unbind_socket(sock)

random.shuffle(all_tests)
all_tests = [1680, 4000, 839] + all_tests
main()
