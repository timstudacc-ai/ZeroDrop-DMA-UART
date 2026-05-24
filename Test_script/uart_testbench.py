#!/usr/bin/env python3
"""
================================================================================
 UART HIL Testbench — Binary Packet Protocol Stress Tester
================================================================================
 Target:   STM32F411 with interrupt-driven UART + DMA + Ring Buffer driver
 Protocol: [START: 0xAA] [LEN: 1B] [PAYLOAD: N bytes] [CHECKSUM: XOR 1B]
 Author:   Auto-generated HIL Testbench
 Python:   >= 3.8
================================================================================
"""

from __future__ import annotations

import struct
import sys
import time
from dataclasses import dataclass, field
from typing import Optional, Tuple

import serial
import serial.tools.list_ports


# ══════════════════════════════════════════════════════════════════════════════
# Constants
# ══════════════════════════════════════════════════════════════════════════════

START_MARKER: int = 0xAA
ESCAPE_BYTE: int = 0x55
DEFAULT_BAUD: int = 9600
DEFAULT_TIMEOUT: float = 1.0
MAX_PAYLOAD_LEN: int = 255


# ══════════════════════════════════════════════════════════════════════════════
# Data Classes
# ══════════════════════════════════════════════════════════════════════════════

@dataclass
class TestMetrics:
    total_sent: int = 0
    successful: int = 0
    checksum_errors: int = 0
    timeouts: int = 0
    bytes_transmitted: int = 0
    start_time: float = field(default_factory=time.perf_counter)
    end_time: float = 0.0

    @property
    def elapsed_seconds(self) -> float:
        end = self.end_time if self.end_time else time.perf_counter()
        return max(end - self.start_time, 1e-9)

    @property
    def packets_per_second(self) -> float:
        return self.total_sent / self.elapsed_seconds

    @property
    def effective_throughput_bps(self) -> float:
        return (self.bytes_transmitted * 8) / self.elapsed_seconds

    @property
    def success_rate_pct(self) -> float:
        return (self.successful / self.total_sent * 100.0) if self.total_sent else 0.0

    @property
    def error_rate_pct(self) -> float:
        return (self.checksum_errors / self.total_sent * 100.0) if self.total_sent else 0.0


# ══════════════════════════════════════════════════════════════════════════════
# UART Testbench Class
# ══════════════════════════════════════════════════════════════════════════════

class UARTTestbench:
    def __init__(
        self,
        port: str,
        baudrate: int = DEFAULT_BAUD,
        timeout: float = DEFAULT_TIMEOUT,
        use_escaping: bool = False,
    ) -> None:
        self._port_name = port
        self._baudrate = baudrate
        self._timeout = timeout
        self._use_escaping = use_escaping
        self._serial: Optional[serial.Serial] = None

        self._open_port()

    def _open_port(self) -> None:
        self._serial = serial.Serial(
            port=self._port_name,
            baudrate=self._baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self._timeout,
            write_timeout=self._timeout,
        )
        self._serial.reset_input_buffer()
        self._serial.reset_output_buffer()

    def close(self) -> None:
        if self._serial and self._serial.is_open:
            self._serial.close()
            print(f"[INFO] Serial port {self._port_name} closed.")

    @staticmethod
    def calculate_xor_checksum(data: bytes | bytearray) -> int:
        crc: int = 0
        for byte in data:
            crc ^= byte
        return crc & 0xFF

    def build_packet(self, payload: bytes | bytearray) -> bytearray:
        if len(payload) > MAX_PAYLOAD_LEN:
            raise ValueError(f"Payload too large: {len(payload)} bytes")

        length_byte = struct.pack("B", len(payload))
        checksum = struct.pack("B", self.calculate_xor_checksum(payload))

        body = bytearray(length_byte) + bytearray(payload) + bytearray(checksum)
        packet = bytearray([START_MARKER]) + body
        return packet

    def send_packet(self, payload: bytes | bytearray) -> int:
        packet = self.build_packet(payload)
        bytes_written = self._serial.write(packet)
        self._serial.flush()
        return bytes_written

    def receive_packet(self) -> Tuple[bool, Optional[bytearray], str]:
        """
        Reads from serial and attempts to parse a valid binary packet.
        Returns: (is_valid, payload, debug_msg)
        """
        start_time = time.perf_counter()
        found_start = False
        while (time.perf_counter() - start_time) < self._timeout:
            byte = self._serial.read(1)
            if not byte:
                return False, None, "TIMEOUT waiting for START_MARKER"
            if byte[0] == START_MARKER:
                found_start = True
                break

        if not found_start:
            return False, None, "TIMEOUT waiting for START_MARKER"

        len_byte = self._serial.read(1)
        if not len_byte:
            return False, None, "TIMEOUT waiting for LENGTH"
        
        payload_len = len_byte[0]
        
        expected_remaining = payload_len + 1
        body = bytearray()
        
        while len(body) < expected_remaining:
            chunk = self._serial.read(expected_remaining - len(body))
            if not chunk:
                return False, None, f"TIMEOUT reading payload. Got {len(body)}/{expected_remaining} bytes"
            body.extend(chunk)

        payload = body[:-1]
        received_crc = body[-1]
        calculated_crc = self.calculate_xor_checksum(payload)

        if received_crc != calculated_crc:
            return False, payload, f"CRC ERROR: Expected 0x{calculated_crc:02X}, got 0x{received_crc:02X}"

        return True, payload, "OK"

    def drain_input(self) -> None:
        if self._serial and self._serial.is_open:
            self._serial.reset_input_buffer()

    def stress_test(
        self,
        num_packets: int = 1000,
        payload_size: int = 8,
        mode: str = "binary",
        modifiers: list = None,
        verbose: bool = False
    ) -> TestMetrics:
        
        if modifiers is None:
            modifiers = []
        
        has_overflow = "overflow" in modifiers
        has_burst = "burst" in modifiers
        has_noise = "noise" in modifiers
        has_fragmented = "fragmented" in modifiers

        if payload_size < 1:
            raise ValueError(f"payload_size must be >= 1")

        if mode == "string" and payload_size > 110 and not has_overflow:
            print("\n[WARNING] String mode sends 2 packets back ('Echo: ' and your string).")
            print(f"To avoid MCU TX ring buffer (128B) overflow, payload size is capped at 110.")
            payload_size = 110

        metrics = TestMetrics()
        self.drain_input()

        print()
        print("=" * 72)
        print("  UART STRESS TEST — BINARY PACKET PROTOCOL")
        print("=" * 72)
        print(f"  Port:           {self._port_name}")
        print(f"  Baud Rate:      {self._baudrate}")
        print(f"  Test Mode:      {mode.upper()}")
        print(f"  Modifiers:      {', '.join(modifiers).upper() if modifiers else 'NONE'}")
        print(f"  Transactions:   {num_packets}")
        print(f"  Payload Size:   {'150 (OVERFLOW)' if has_overflow else payload_size} bytes")
        print(f"  Verbose Mode:   {'YES' if verbose else 'NO'}")
        print("=" * 72)
        print()

        progress_interval = max(1, num_packets // 20)
        import random

        burst_size = 5 if has_burst else 1

        for i in range(num_packets):
            tx_data = bytearray()
            payloads = []

            for b_idx in range(burst_size):
                actual_i = i * burst_size + b_idx
                current_payload_size = 150 if has_overflow else payload_size

                if mode == "binary":
                    payload = bytearray(((actual_i + j) & 0xFF) for j in range(current_payload_size))
                else:
                    if actual_i % 100 == 10:
                        payload_str = "LED_ON"
                    elif actual_i % 100 == 40:
                        payload_str = "TEST_CAPACITY"
                    elif actual_i % 100 == 60:
                        payload_str = "LED_OFF"
                    else:
                        payload_str = f"Msg#{actual_i}".ljust(current_payload_size, "_")
                    payload_str = payload_str[:current_payload_size]
                    payload = payload_str.encode('ascii')

                payloads.append(payload)
                
                try:
                    packet = self.build_packet(payload)
                except ValueError as e:
                    # MAX_PAYLOAD_LEN is 255, overflow is 150, so this won't raise, but just in case
                    packet = bytearray()

                if has_noise:
                    noise_before = bytearray(random.randint(0, 255) for _ in range(5))
                    noise_after = bytearray(random.randint(0, 255) for _ in range(5))
                    packet = noise_before + packet + noise_after

                tx_data.extend(packet)

            if verbose:
                print(f"\n[Transaction #{i+1}] Sending {len(tx_data)} bytes in burst of {burst_size} packets...")

            try:
                if has_fragmented:
                    for byte in tx_data:
                        self._serial.write(bytes([byte]))
                        self._serial.flush()
                        time.sleep(0.002)
                    bytes_written = len(tx_data)
                else:
                    bytes_written = self._serial.write(tx_data)
                    self._serial.flush()
                
                metrics.bytes_transmitted += bytes_written

                # Wait for response(s) for EACH payload in the burst
                for p_idx, payload in enumerate(payloads):
                    metrics.total_sent += 1
                    
                    if mode == "string":
                        is_cmd = payload.decode('ascii') in ("LED_ON", "LED_OFF", "TEST_CAPACITY")
                        if is_cmd:
                            is_valid, rx_payload, debug_msg = self.receive_packet()
                            if is_valid:
                                rx_str = rx_payload.decode('ascii', errors='replace')
                                if payload.decode('ascii') == "LED_ON":
                                    expected_str = "LED is now ON"
                                elif payload.decode('ascii') == "LED_OFF":
                                    expected_str = "LED is now OFF"
                                else:
                                    expected_str = "C" * 110
                                    
                                if rx_str == expected_str:
                                    metrics.successful += 1
                                    if verbose: print(f"  [{p_idx+1}/{burst_size}] Command OK! Got: '{rx_str}'")
                                else:
                                    metrics.checksum_errors += 1
                                    if verbose: print(f"  [{p_idx+1}/{burst_size}] BAD RESPONSE. Expected '{expected_str}', got '{rx_str}'")
                            else:
                                metrics.timeouts += 1
                                if verbose: print(f"  [{p_idx+1}/{burst_size}] ERROR: {debug_msg}")
                        else:
                            is_valid1, rx1, msg1 = self.receive_packet()
                            is_valid2, rx2, msg2 = self.receive_packet()
                            
                            if is_valid1 and is_valid2:
                                str1 = rx1.decode('ascii', errors='replace')
                                str2 = rx2.decode('ascii', errors='replace')
                                if str1 == "Echo: " and str2 == payload.decode('ascii'):
                                    metrics.successful += 1
                                    if verbose: print(f"  [{p_idx+1}/{burst_size}] Echo OK! Got: '{str1}' and '{str2}'")
                                else:
                                    metrics.checksum_errors += 1
                                    if verbose: print(f"  [{p_idx+1}/{burst_size}] MISMATCH. Got '{str1}' & '{str2}'")
                            else:
                                if "TIMEOUT" in msg1 or "TIMEOUT" in msg2:
                                    metrics.timeouts += 1
                                else:
                                    metrics.checksum_errors += 1
                                if verbose: print(f"  [{p_idx+1}/{burst_size}] RX FAIL. Pkt 1: {msg1}, Pkt 2: {msg2}")
                    else:
                        is_valid, rx_payload, debug_msg = self.receive_packet()
                        if is_valid:
                            if rx_payload == payload:
                                metrics.successful += 1
                                if verbose: print(f"  [{p_idx+1}/{burst_size}] Echo OK! Payload: {rx_payload.hex(' ').upper()}")
                            else:
                                metrics.checksum_errors += 1
                                if verbose: print(f"  [{p_idx+1}/{burst_size}] DATA MISMATCH! Expected {payload.hex(' ').upper()}")
                        else:
                            if "TIMEOUT" in debug_msg:
                                metrics.timeouts += 1
                            else:
                                metrics.checksum_errors += 1
                            if verbose: print(f"  [{p_idx+1}/{burst_size}] {debug_msg}")

            except serial.SerialException as exc:
                metrics.total_sent += burst_size
                metrics.timeouts += burst_size
                if verbose:
                    print(f"\n[ERROR] Serial exception on transaction #{i + 1}: {exc}")

            if not verbose and ((i + 1) % progress_interval == 0 or (i + 1) == num_packets):
                pct = (i + 1) / num_packets * 100
                bar_len = 30
                filled = int(bar_len * (i + 1) / num_packets)
                bar = "█" * filled + "░" * (bar_len - filled)
                sys.stdout.write(
                    f"\r  Progress: [{bar}] {pct:5.1f}%  "
                    f"({i + 1}/{num_packets})  "
                    f"OK:{metrics.successful} "
                    f"ERR:{metrics.checksum_errors} "
                    f"TMO:{metrics.timeouts}"
                )
                sys.stdout.flush()

        metrics.end_time = time.perf_counter()
        if not verbose:
            print("\n")
        return metrics


def print_report(metrics: TestMetrics) -> None:
    print("╔" + "═" * 60 + "╗")
    print("║" + "  STRESS TEST REPORT".center(60) + "║")
    print("╠" + "═" * 60 + "╣")
    print(f"║  Total Packets Sent:        {metrics.total_sent:>10,}          ║")
    print(f"║  Successful Transactions:   {metrics.successful:>10,}          ║")
    print(f"║  Data/Checksum Errors:      {metrics.checksum_errors:>10,}          ║")
    print(f"║  Timeouts (no response):    {metrics.timeouts:>10,}          ║")
    print("╠" + "═" * 60 + "╣")
    print(f"║  Success Rate:              {metrics.success_rate_pct:>9.2f}%          ║")
    print(f"║  Error Rate:                {metrics.error_rate_pct:>9.2f}%          ║")
    print(f"║  Total Bytes TX:            {metrics.bytes_transmitted:>10,}          ║")
    print(f"║  Elapsed Time:              {metrics.elapsed_seconds:>9.3f}s          ║")
    print(f"║  Throughput:                {metrics.packets_per_second:>9.1f} pkt/s     ║")
    print(f"║  Effective Bitrate:         {metrics.effective_throughput_bps:>9.0f} bps      ║")
    print("╚" + "═" * 60 + "╝")
    print()


def select_com_port() -> str:
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("[FATAL] No COM ports detected. Check your USB cable and drivers.")
        sys.exit(1)

    print("\n╔" + "═" * 60 + "╗")
    print("║" + "  AVAILABLE SERIAL PORTS".center(60) + "║")
    print("╠" + "═" * 60 + "╣")

    for idx, port_info in enumerate(ports, start=1):
        desc = f"{port_info.device}  —  {port_info.description}"
        if len(desc) > 56:
            desc = desc[:53] + "..."
        print(f"║  [{idx}]  {desc:<54}║")
    print("╚" + "═" * 60 + "╝\n")

    while True:
        try:
            choice = input("  Select port number: ").strip()
            index = int(choice) - 1
            if 0 <= index < len(ports):
                selected = ports[index].device
                print(f"  → Selected: {selected}\n")
                return selected
            else:
                print(f"  [!] Enter a number between 1 and {len(ports)}.")
        except (ValueError, EOFError):
            print("  [!] Invalid input. Enter a numeric port number.")


def get_test_config() -> dict:
    print("╔" + "═" * 60 + "╗")
    print("║" + "  TEST CONFIGURATION".center(60) + "║")
    print("╠" + "═" * 60 + "╣")
    print("║  Press ENTER to accept [default] values.                  ║")
    print("╚" + "═" * 60 + "╝\n")

    def ask_int(prompt: str, default: int, min_val: int = 1, max_val: int = 100_000) -> int:
        while True:
            raw = input(f"  {prompt} [{default}]: ").strip()
            if not raw:
                return default
            try:
                val = int(raw)
                if min_val <= val <= max_val:
                    return val
                print(f"  [!] Value must be between {min_val} and {max_val}.")
            except ValueError:
                print("  [!] Please enter a valid integer.")

    def ask_bool(prompt: str, default: bool) -> bool:
        default_str = "Y/n" if default else "y/N"
        raw = input(f"  {prompt} [{default_str}]: ").strip().lower()
        if not raw:
            return default
        return raw in ("y", "yes", "1", "true")
        
    def ask_choice(prompt: str, choices: list, default: str) -> str:
        choices_str = "/".join(choices)
        while True:
            raw = input(f"  {prompt} ({choices_str}) [{default}]: ").strip().lower()
            if not raw:
                return default.lower()
            if raw in (c.lower() for c in choices):
                return raw
            print(f"  [!] Please select one of: {choices_str}")

    mode = ask_choice("Test Mode", ["Binary", "String"], "String")
    
    modifiers_raw = input("  Chaos Modifiers (comma-separated: Fragmented,Burst,Noise,Overflow) [None]: ").strip().lower()
    modifiers = [m.strip() for m in modifiers_raw.split(',')] if modifiers_raw else []
    
    num_packets = ask_int("Number of transactions (bursts) to send", 1000, 1, 1_000_000)
    payload_size = ask_int("Payload size (bytes)", 8, 1, MAX_PAYLOAD_LEN)
    baudrate = ask_int("Baud rate", DEFAULT_BAUD, 1200, 3_000_000)
    verbose = ask_bool("Enable verbose debug visualization?", False)

    print()
    return {
        "mode": mode,
        "modifiers": modifiers,
        "num_packets": num_packets,
        "payload_size": payload_size,
        "baudrate": baudrate,
        "verbose": verbose,
    }


def main() -> None:
    print("\n  ┌─────────────────────────────────────────────────────────┐")
    print("  │     UART HIL TESTBENCH  —  Binary Packet Protocol      │")
    print("  │     STM32F411  •  Ring Buffer  •  DMA + IDLE IRQ        │")
    print("  └─────────────────────────────────────────────────────────┘\n")

    port = select_com_port()
    config = get_test_config()
    testbench: Optional[UARTTestbench] = None

    try:
        testbench = UARTTestbench(
            port=port,
            baudrate=config["baudrate"],
            timeout=DEFAULT_TIMEOUT,
            use_escaping=False,
        )

        print(f"  [OK] Port {port} opened at {config['baudrate']} baud.\n")

        input("  Press ENTER to start stress test...")

        metrics = testbench.stress_test(
            num_packets=config["num_packets"],
            payload_size=config["payload_size"],
            mode=config["mode"],
            modifiers=config["modifiers"],
            verbose=config["verbose"],
        )

        print_report(metrics)

    except serial.SerialException as exc:
        print(f"\n  [FATAL] Could not open serial port: {exc}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n  [!] Test aborted by user (Ctrl+C).")
    finally:
        if testbench is not None:
            testbench.close()


if __name__ == "__main__":
    main()
