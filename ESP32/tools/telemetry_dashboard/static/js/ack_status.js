const COMMAND_ACK_LABELS = Object.freeze({
  1: "accepted",
  2: "applied",
  3: "failed",
  4: "timeout",
  5: "rejected",
});

const ACK_TERMINAL_STATES = new Set([2, 3, 4, 5]);

export function commandAckLabel(value) {
  return COMMAND_ACK_LABELS[value | 0] || "none";
}

export function isTerminalAck(value) {
  return ACK_TERMINAL_STATES.has(value | 0);
}
