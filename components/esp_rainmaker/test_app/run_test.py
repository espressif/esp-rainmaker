#!/usr/bin/env python3
import argparse
import time
import re
import sys
from serial import serial_for_url, SerialException
from datetime import datetime

def parse_args():
    p = argparse.ArgumentParser(description="Toggle DTR/RTS, send '*' + Enter, and wait for label.")
    p.add_argument("port", help="Serial port (e.g. rfc://host:port, /dev/ttyUSB0, or COM3)")
    p.add_argument("-b", "--baud", type=int, default=115200, help="Baudrate (default 115200)")
    p.add_argument("-l", "--label", required=True, help="Label to wait for (substring or regex)")
    p.add_argument("--regex", action="store_true", help="Treat label as regular expression")
    p.add_argument("--pulse-duration", type=float, default=0.5, help="DTR/RTS pulse duration in seconds (default 0.5)")
    p.add_argument("--pre-pulse-delay", type=float, default=0.1, help="Delay before toggling DTR/RTS (default 0.1s)")
    p.add_argument("--post-pulse-delay", type=float, default=0.1, help="Delay after toggling before sending '*' (default 0.1s)")
    p.add_argument("--read-timeout", type=float, default=600.0, help="Timeout waiting for label (0 = infinite)")
    p.add_argument("--no-toggle", action="store_true", help="Skip DTR/RTS toggle")
    p.add_argument("--log-file", help="Save serial output to this file (default: auto-named with timestamp)")
    p.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    return p.parse_args()

def verbose_print(enabled, *args, **kwargs):
    if enabled:
        print(*args, **kwargs)

def main():
    args = parse_args()

    url = args.port
    if url.startswith("rfc://"):
        url = url.replace("rfc://", "rfc2217://", 1)

    baud = args.baud
    label = args.label
    pulse_duration = args.pulse_duration
    pre_delay = args.pre_pulse_delay
    post_delay = args.post_pulse_delay
    read_timeout = args.read_timeout
    verbose = args.verbose

    verbose_print(verbose, f"Opening serial port: {url} @ {baud} baud...")

    try:
        ser = serial_for_url(url, baudrate=baud, timeout=1)
    except SerialException as e:
        print(f"ERROR: could not open serial port {url}: {e}", file=sys.stderr)
        sys.exit(2)

    verbose_print(verbose, "Opened serial port successfully.")

    # Prepare log file
    log_file = args.log_file
    if not log_file:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = f"serial_log_{timestamp}.log"
    try:
        log_fh = open(log_file, "w", encoding="utf-8")
        verbose_print(verbose, f"Logging serial output to: {log_file}")
    except Exception as e:
        print(f"ERROR: cannot open log file {log_file}: {e}", file=sys.stderr)
        sys.exit(5)

    try:
        # Toggle DTR/RTS if not skipped
        if not args.no_toggle:
            verbose_print(verbose, f"Waiting {pre_delay}s before toggling DTR/RTS...")
            time.sleep(pre_delay)

            verbose_print(verbose, "Pulsing DTR/RTS...")
            try:
                ser.setDTR(False)
                ser.setRTS(False)
            except Exception:
                try:
                    ser.dtr = False
                    ser.rts = False
                except Exception:
                    verbose_print(verbose, "Warning: unable to toggle DTR/RTS on this port.")
            time.sleep(pulse_duration)

            try:
                ser.setDTR(True)
                ser.setRTS(True)
            except Exception:
                try:
                    ser.dtr = True
                    ser.rts = True
                except Exception:
                    pass
            verbose_print(verbose, f"Pulsed DTR/RTS for {pulse_duration}s. Waiting {post_delay}s...")
            time.sleep(post_delay)
        else:
            verbose_print(verbose, "Skipping DTR/RTS toggle (--no-toggle used).")

        # Wait for device: either menu prompt (interactive) or completion label (auto-run mode)
        menu_prompt = "Press ENTER to see the list of tests"
        verbose_print(verbose, f"Waiting for device prompt '{menu_prompt}' or label '{label}' (timeout 90s)...")
        wait_start = time.time()
        prompt_buffer = ""
        auto_completed = False
        while (time.time() - wait_start) < 90:
            chunk = ser.read(1024)
            if chunk:
                text = chunk.decode("utf-8", errors="replace")
                sys.stdout.write(text)
                sys.stdout.flush()
                log_fh.write(text)
                log_fh.flush()
                prompt_buffer += text
                if len(prompt_buffer) > 5000:
                    prompt_buffer = prompt_buffer[-5000:]
                if label in prompt_buffer:
                    verbose_print(verbose, "Label seen (auto-run completed).")
                    auto_completed = True
                    break
                if menu_prompt in prompt_buffer:
                    verbose_print(verbose, "Prompt seen, sending keypresses.")
                    break
            time.sleep(0.05)
        else:
            verbose_print(verbose, "Prompt not seen in time; sending keypresses anyway.")

        if auto_completed:
            print(f"\n✅ Label '{label}' detected. Exiting successfully.")
        else:
            # Send Enter to show the test menu, then '*' + Enter to run all tests (script drives device; no manual input)
            time.sleep(0.3)
            verbose_print(verbose, "Sending Enter to show menu...")
            ser.write(b'\r\n')
            ser.flush()
            time.sleep(1.5)
            verbose_print(verbose, "Sending '*' to run all tests, then Enter...")
            ser.write(b'*')  # Unity: * = run all tests
            ser.flush()
            time.sleep(0.1)
            ser.write(b'\r\n')
            ser.flush()
            verbose_print(verbose, "Done. Waiting for test output and end label.")

            # Prepare label matcher
            if args.regex:
                pattern = re.compile(label)
                match_fn = lambda s: bool(pattern.search(s))
            else:
                match_fn = lambda s: (label in s)

            buffer = ""
            start_time = time.time()
            verbose_print(verbose, "Reading serial output (Ctrl+C to abort)...")

            while True:
                if read_timeout > 0 and (time.time() - start_time) > read_timeout:
                    print(f"\nERROR: Timeout after {read_timeout}s waiting for '{label}'", file=sys.stderr)
                    break

                chunk = ser.read(1024)
                if not chunk:
                    continue

                text = chunk.decode('utf-8', errors='replace')
                sys.stdout.write(text)
                sys.stdout.flush()
                log_fh.write(text)
                log_fh.flush()

                buffer += text
                if len(buffer) > 20000:
                    buffer = buffer[-20000:]

                if match_fn(buffer):
                    print(f"\n✅ Label '{label}' detected. Exiting successfully.")
                    break

    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    finally:
        ser.close()
        log_fh.close()
        verbose_print(verbose, "Serial port closed and log saved.")

if __name__ == "__main__":
    main()
