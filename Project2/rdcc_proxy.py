import socket
import threading
import time
import argparse
import random
import queue

class RandomLoss:
    def __init__(self, loss_rate, random_seed):
        self.loss_rate = loss_rate
        random.seed(random_seed)

    def forward_success(self):
        return random.random() >= self.loss_rate


class TokenBucket:
    def __init__(self, rate, capacity, max_queue_size):
        self.tokens = capacity
        self.rate = rate
        self.capacity = capacity
        self.lock = threading.Lock()
        self.q = queue.Queue(maxsize=max_queue_size)
        
        # start the token refill thread
        threading.Thread(target=self.refill).start()
        
    def refill(self):
        while True:
            with self.lock:
                if self.tokens < self.capacity:
                    self.tokens += 1
            time.sleep(1 / self.rate)
            
    def consume(self):
        with self.lock:
            if self.tokens > 0:
                self.tokens -= 1
                return True
            else:
                return False

    def enqueue(self, item):
        if not self.q.full():
            self.q.put(item)
            return True
        else:
            return False

    def dequeue(self):
        # if queue is empty, return None
        if self.q.empty():
            return None
        
        # if queue is not empty, try to consume a token
        if self.consume():
            item = self.q.get()
            return item
        else:
            return None

def producer(bucket, listen_port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(('localhost', listen_port))
        print(f'Listening on port {listen_port}...')
        while True:
            data, addr = s.recvfrom(4096)
            if len(data) > 1200:
                # drop oversized packets
                continue
            # print(f'Packet received: {data}')
            success = bucket.enqueue(data)
            # if success:
            #     print(f'Packet enqueued: {data}, queue size: {bucket.q.qsize()}')
            # else:
            #     print(f'Packet dropped: {data}')

def delayed_send(data, addr, port, delay):
    time.sleep(delay)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(data, (addr, port))
            
def consumer(bucket, forward_port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        while True:
            data = bucket.dequeue()
            if data:
                # print(f'Packet dequeued: {data}, queue size: {bucket.q.qsize()}')
                threading.Thread(target=delayed_send, args=(data, 'localhost', forward_port, args.prop_delay)).start()


def forward_packets(listen_port, forward_port, test_type):
    if test_type == 'rd':
        tunnel = RandomLoss(args.loss_rate, args.random_seed)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind(('localhost', listen_port))
            print(f'Listening on port {listen_port}...')
            while True:
                data, addr = s.recvfrom(4096)
                if len(data) > 1200:
                    # drop oversized packets
                    continue
                if tunnel.forward_success():
                    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as forward_socket:
                        forward_socket.sendto(data, ('localhost', forward_port))
                else:
                    print(f'Packet dropped on port {listen_port}')
    
    elif test_type == 'cc':
        bucket = TokenBucket(args.token_rate, args.token_capacity, args.queue_size)
        threading.Thread(target=producer, args=(bucket, listen_port)).start()
        threading.Thread(target=consumer, args=(bucket, forward_port)).start()


def main():
    threading.Thread(target=forward_packets, args=(args.in_port_from_client, args.server_port, args.test_type)).start()
    threading.Thread(target=forward_packets, args=(args.in_port_from_server, args.client_port, args.test_type)).start()
    while 1:
        time.sleep(1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--in_port_from_client', '-d', type=int, default=5002)
    parser.add_argument('--in_port_from_server', '-a', type=int, default=5001)
    parser.add_argument('--client_port', '-c', type=int, default=6001)
    parser.add_argument('--server_port', '-s', type=int, default=6002)
    parser.add_argument('--test_type', '-t', type=str, choices=['rd', 'cc'], default='rd')
    parser.add_argument('--loss_rate', '-l', type=float, default=0.1)
    parser.add_argument('--token_rate', '-k', type=int, default=100)
    parser.add_argument('--token_capacity', '-b', type=int, default=1)
    parser.add_argument('--queue_size', '-q', type=int, default=30)
    parser.add_argument('--random_seed', '-r', type=int, default=0)
    parser.add_argument('--prop_delay', '-p', type=float, default=0.1)
    
    args = parser.parse_args()
    main()
    