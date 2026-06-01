import argparse

from dashboard_server import build_dashboard_server
from dashboard_state import DashboardState
from serial_teleop_bridge import SerialBridgeConfig, SerialTeleopBridge
from telemetry_client import TelemetryClient
from telemetry_serial_client import TelemetrySerialClient


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
        "--enable-com-passthrough",
        action="store_true",
        help="Optional raw leader COM -> follower COM passthrough (debug only)",
    )
    parser.add_argument("--follower-com", default="COM8")
    args = parser.parse_args()

    state = DashboardState()
    if args.leader_serial:
        telemetry_client = TelemetrySerialClient(args.leader_serial, args.leader_serial_baud, state)
    else:
        telemetry_client = TelemetryClient(args.leader_host, args.leader_port, state)
    telemetry_client.start()

    com_passthrough = None
    if args.enable_com_passthrough:
        com_passthrough = SerialTeleopBridge(SerialBridgeConfig(follower_port=args.follower_com))

    server = build_dashboard_server(
        bind_host="0.0.0.0",
        port=args.dashboard_port,
        state=state,
        command_sender=telemetry_client.send_command,
        com_passthrough=com_passthrough,
    )

    print(f"Dashboard: http://127.0.0.1:{args.dashboard_port}")
    if args.leader_serial:
        print(f"ESP source: serial {args.leader_serial} @ {args.leader_serial_baud}")
    else:
        print(f"ESP source: {args.leader_host}:{args.leader_port}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if com_passthrough is not None:
            com_passthrough.stop()
        telemetry_client.stop_event.set()
        server.server_close()


if __name__ == "__main__":
    main()
