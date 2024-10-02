import sys
import struct
import mido
import socket

octave = 0

"""
Request
0x00: Note ON
    [MIDI Note: 1byte]
0x01: Note OFF
    [MIDI Note: 1byte]
0x02: Note All Stop
0x03: Version
    [Recieve Port: 2bytes]

Response
0x03: Version
    [Server Version: 1byte]
    [Protocol Version: 1byte]
"""

class MIDIPlayer:
    def __init__(self, address: str, port: int):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.address = address
        self.port = port
        self.enableLog = False

    def noteOn(self, note):
        if self.enableLog:
            print("noteOn: " + str(note))
        self._sendPacket(b"\x00" + struct.pack("B", note))

    def noteOff(self, note):
        if self.enableLog:
            print("noteOff: " + str(note))
        self._sendPacket(b"\x01" + struct.pack("B", note))

    def noteStop(self):
        self._sendPacket(b"\x02")

    def _sendPacket(self, data):
        self.socket.sendto(data, (self.address, self.port))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.noteStop()
        self.socket.close()


if len(sys.argv[1:]) == 0:
    if len(mido.get_input_names()) == 0:
        print("MIDI input device not found")
        sys.exit()
    else:
        inputs = []
        for input_name in mido.get_input_names():
            inputs.append(str(len(inputs) + 1) + ": " + input_name)
        print("\n".join(inputs))
        input_num = input("MIDI Input > ")
        print()
        midi_source = mido.open_input(mido.get_input_names()[int(input_num) - 1])
else:
    print("Playing \"" + sys.argv[1] + "\"...")
    midi_source = mido.MidiFile(sys.argv[1]).play()
    if len(sys.argv[1:]) == 2:
        octave = int(sys.argv[2])


with MIDIPlayer("192.168.0.10", 50000) as player:
    for msg in midi_source:
        if msg.type == "note_off":
            player.noteOff(msg.note + octave * 12)
        if msg.type == "note_on":
            if msg.velocity == 0:
                player.noteOff(msg.note + octave * 12)
            else:
                player.noteOn(msg.note + octave * 12)
