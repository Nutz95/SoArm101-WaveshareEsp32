import argparse

from dashboard_server import build_dashboard_server
from dashboard_state import DashboardState
from telemetry_client import TelemetryClient


def main() -> None:
    parser = argparse.ArgumentParser(description="SoArm telemetry dashboard")
    parser.add_argument("--leader-host", default="soarm-leader.local")
    parser.add_argument("--leader-port", type=int, default=9090)
    parser.add_argument("--dashboard-port", type=int, default=8080)
    args = parser.parse_args()

    state = DashboardState()
    telemetry_client = TelemetryClient(args.leader_host, args.leader_port, state)
    telemetry_client.start()

    server = build_dashboard_server(
        bind_host="0.0.0.0",
        port=args.dashboard_port,
        state=state,
        command_sender=telemetry_client.send_command,
    )

    print(f"Dashboard: http://127.0.0.1:{args.dashboard_port}")
    print(f"ESP source: {args.leader_host}:{args.leader_port}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        telemetry_client.stop_event.set()
        server.server_close()


if __name__ == "__main__":
    main()
