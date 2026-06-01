import argparse
import json
from pathlib import Path

from dashboard_server import build_dashboard_server
from dashboard_state import DashboardState
from serial_teleop_bridge import SerialBridgeConfig, SerialTeleopBridge
from telemetry_client import TelemetryClient
from telemetry_serial_client import TelemetrySerialClient
from telemetry_com_mirror import ComMirrorConfig, TelemetryComMirror


def load_com_mirror_config(path: Path) -> ComMirrorConfig:
    if not path.is_file():
        return ComMirrorConfig()
    with path.open("r", encoding="utf-8") as handle:
        raw = json.load(handle)
    return ComMirrorConfig(
        follower_port=str(raw.get("follower_port", "COM8")),
        follower_baud=int(raw.get("follower_baud", 115200)),
        speed_pct=int(raw.get("speed_pct", 100)),
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="SoArm telemetry dashboard")
    parser.add_argument("--leader-host", default="soarm-leader.local")
    parser.add_argument("--leader-port", type=int, default=9090)
    parser.add_argument(
        "--leader-serial",
        default="",
        help="Use USB serial debug instead of Wi-Fi (e.g. COM7). Same protocol as :9090.",
    )
    parser.add_argument("--leader-serial-baud", type=int, default=115200)
    parser.add_argument("--dashboard-port", type=int, default=8080)
    parser.add_argument(
        "--enable-com-mirror",
        action="store_true",
        help="Enable WiFi telemetry -> follower COM mirror (requires pyserial)",
    )
    parser.add_argument(
        "--enable-serial-bridge",
        action="store_true",
        help="Alias for --enable-com-mirror",
    )
    parser.add_argument("--follower-com", default="COM8")
    parser.add_argument("--serial-bridge-follower-com", default="")
    parser.add_argument("--follower-baud", type=int, default=115200)
    parser.add_argument(
        "--auto-start-com-mirror",
        action="store_true",
        help="Open follower COM and start sending batches immediately",
    )
    parser.add_argument(
        "--auto-start-serial-bridge",
        action="store_true",
        help="Alias for --auto-start-com-mirror",
    )
    parser.add_argument(
        "--enable-com-passthrough",
        action="store_true",
        help="Optional raw leader COM -> follower COM passthrough (debug only)",
    )
    args = parser.parse_args()

    state = DashboardState()
    if args.leader_serial:
        telemetry_client = TelemetrySerialClient(args.leader_serial, args.leader_serial_baud, state)
    else:
        telemetry_client = TelemetryClient(args.leader_host, args.leader_port, state)
    telemetry_client.start()

    config_path = Path(__file__).resolve().parent / "serial_bridge_config.json"
    follower_com = args.serial_bridge_follower_com or args.follower_com
    com_mirror = None
    com_passthrough = None

    if args.enable_com_mirror or args.enable_serial_bridge:
        config = load_com_mirror_config(config_path)
        if follower_com:
            config.follower_port = follower_com
        config.follower_baud = args.follower_baud
        com_mirror = TelemetryComMirror(state, config)
        if args.auto_start_com_mirror or args.auto_start_serial_bridge:
            try:
                com_mirror.start()
            except Exception as exc:
                print(f"[com-mirror] auto-start failed: {exc}")

    if args.enable_com_passthrough:
        com_passthrough = SerialTeleopBridge(SerialBridgeConfig(follower_port=follower_com))

    server = build_dashboard_server(
        bind_host="0.0.0.0",
        port=args.dashboard_port,
        state=state,
        command_sender=telemetry_client.send_command,
        com_mirror=com_mirror,
        com_passthrough=com_passthrough,
    )

    print(f"Dashboard: http://127.0.0.1:{args.dashboard_port}")
    if args.leader_serial:
        print(f"ESP source: serial {args.leader_serial} @ {args.leader_serial_baud}")
    else:
        print(f"ESP source: {args.leader_host}:{args.leader_port}")
    if com_mirror is not None:
        print(f"COM mirror ready for follower port {com_mirror.snapshot().get('follower_port', follower_com)}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if com_mirror is not None:
            com_mirror.stop()
        if com_passthrough is not None:
            com_passthrough.stop()
        telemetry_client.stop_event.set()
        server.server_close()


if __name__ == "__main__":
    main()
