#!/usr/bin/env python3
"""Serial to File

This command line script listens for data on a serial port and writes it to a
file. This is useful for collecting sensor data from an Arduino for analysis or
other processing.

"""
import argparse
from serial import Serial
from serial.tools.list_ports import comports
from datetime import datetime
from time import sleep


def main():
    # Get command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--baudrate', type=int, default=9600,
        help='Baud rate for serial interface')
    parser.add_argument('-o', '--output_dir', type=str, default='./',
        help='Directory for output file.')
    args = parser.parse_args()

    # Get serial port
    available_ports = comports()
    if not available_ports:
        print('\nNo serial ports found.\n')
        return
    port = get_port(available_ports)
    if not port:
        return

    # Start listening
    ser = Serial(port, baudrate=args.baudrate, timeout=0.5)
    print(f'\nListening on port {port} [Press CTRL-C to quit]')
    print(f'  Baud rate: {ser.baudrate}')
    print(f'  Byte size: {ser.bytesize}')
    print(f'  Parity: {ser.parity}')
    print(f'  Stop bits: {ser.stopbits}')

    fname = 'serial_data_' + datetime.now().strftime('%Y%m%d%H%M%S') + '.txt'
    print(f'  Output file: {fname}\n')

    # Write to file
    with open(args.output_dir + fname, 'wb') as f:
        while True:
            try:
                f.write(ser.readline())
            except KeyboardInterrupt:
                print('Quitting program...\n')
                break


def get_port(available_ports):
    # Return None if there are no available ports
    if not available_ports:
        return None

    # If there are ports, grab the port names form the ListPortInfo objects
    port_names = [port[0] for port in available_ports]

    # Return the port name if there's only one to choose from
    if len(port_names) == 1:
        return port_names[0]

    # If there are more, ask the user which port they want to listen on
    while True:
        failed_validation = False  # Flag for response validation
        print('\nSelect a serial port to listen on.')
        for i, port in enumerate(port_names, 1):
            print(f'  {i}: {port}')
        prompt = '\nEnter a number (1-{}), or type "exit" to exit the program: '
        selection = input(prompt.format(len(port_names)))

        # Expecting an integer OR for the user to type "exit"
        try:
            selection = int(selection)
        except ValueError:
            if selection.lower() == 'exit':
                return None
            failed_validation = True

        # Validation to check if the selection is an int that's out of range
        if selection not in range(1, len(port_names) + 1):
            failed_validation = True

        # Keep prompting the user until they enter a valid response
        if failed_validation:
            print('\nThat\'s not a valid selection.')
            sleep(0.5)
            continue

        return port_names[selection - 1]  # Return the selected port


if __name__ == '__main__':
    main()
