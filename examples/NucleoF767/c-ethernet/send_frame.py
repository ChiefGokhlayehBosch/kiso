import argparse
import binascii
import netifaces
from socket import socket, AF_PACKET, SOCK_RAW


def send(src, dst, eth_type, payload, interface):
    """Send raw Ethernet frame over interface."""

    assert(len(src) == len(dst) == 6)  # 48-bit ethernet addresses
    assert(len(eth_type) == 2)  # 16-bit ethernet type or length

    s = socket(AF_PACKET, SOCK_RAW)

    s.bind((interface, 0))
    return s.send(src + dst + eth_type + payload)


if __name__ == "__main__":
    default_gateway = netifaces.gateways()['default']
    default_interface = default_gateway[netifaces.AF_INET][1]
    default_dst_mac = 'ab:cd:ef:ab:cd:ef'
    default_src_mac = netifaces.ifaddresses(default_interface)[
        netifaces.AF_LINK][0]['addr']
    default_payload = "hello world"

    argparser = argparse.ArgumentParser()
    argparser.add_argument('-d', '--dst', help=f"destination MAC address to use (default={default_dst_mac})",
                           default=default_dst_mac, type=str)
    argparser.add_argument('-s', '--src', help=f"source MAC address to use (default={default_src_mac})",
                           default=None, type=str)
    argparser.add_argument('-i', '--interface', help=f"network-interface to send with, will also affect default source MAC (default={default_interface})",
                           default=default_interface, type=str)
    argparser.add_argument(
        'payload', help=f"payload to put in the frame (default={default_payload})", type=str)

    args = argparser.parse_args()

    dst_mac = binascii.unhexlify(args.dst.replace(':', ''))
    src_mac = binascii.unhexlify((args.src if args.src != None else netifaces.ifaddresses(
        args.interface)[netifaces.AF_LINK][0]['addr']).replace(':', ''))
    payload = args.payload.encode()
    interface = args.interface

    num_bytes_sent = send(dst_mac,
                          src_mac,
                          len(payload).to_bytes(2, byteorder='big'),
                          payload,
                          interface)

    print(f"Sent {num_bytes_sent}-byte Ethernet frame on {interface}")
