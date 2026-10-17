/**
 * Claude Code hook for Agent Sync. Reads the hook payload on stdin.
 * - PreToolUse (Edit / Write / MultiEdit / NotebookEdit): asks the server to check-and-claim the file.
 *   An Edit or MultiEdit inside named declarations claims just those (`src/user.ts#fetchUser`), so two
 *   developers can change different functions in one file; anything else claims the whole file.
 *   If someone else holds it, exits 2 so Claude Code blocks the edit and shows the agent why;
 *   otherwise the file is claimed for this developer, so nobody else's agent can take it mid-edit.
 *   If another developer let go of the file recently (their change is probably not merged yet), the edit is
 *   claimed but Claude Code asks the user before it runs, naming who changed it.
 * - SessionStart: tells the agent its clientSessionId, so its register_session joins up with this window's edits.
 * - SessionEnd: frees this window's claims right away and leaves a handoff note.
 * Each Claude Code window is its own session (keyed by the payload's session_id), so two windows of the same
 * developer block each other like two developers would.
 *
 * Env: AGENT_SYNC_URL, AGENT_SYNC_TOKEN, AGENT_SYNC_DEVELOPER (defaults to `git config user.name`),
 *      AGENT_SYNC_MODE=block|warn
 * If the server is unreachable the hook allows the edit: a down server must never stop work.
 */
import { execFileSync } from "node:child_process";
import path from "node:path";
import type { HookPreEditResult, RecentChange } from "@agent-sync/shared";
import { symbolsForEdit } from "./symbols.ts";

interface HookInput {
  hook_event_name: string;
  session_id?: string;
  tool_name?: string;
  tool_input?: {
    file_path?: string;
    notebook_path?: string;
    old_string?: string;
    edits?: { old_string?: string }[];
  };
  cwd: string;
}

const url = process.env.AGENT_SYNC_URL ?? "http://localhost:4000";
const token = process.env.AGENT_SYNC_TOKEN ?? "change-me";
const mode = process.env.AGENT_SYNC_MODE ?? "block";

async function readStdin(): Promise<string> {
  let data = "";
  for await (const chunk of process.stdin) data += chunk;
  return data;
}

function git(args: string[], cwd: string): string | undefined {
  try {
    return execFileSync("git", args, { cwd, encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] }).trim() || undefined;
  } catch {
    return undefined;
  }
}

/** Same rules as normalizePath in @agent-sync/shared, inlined so the hook runs on plain node with no install. */
function normalizePath(p: string): string {
  return p
    .replace(/\\/g, "/")
    .replace(/^\.\//, "")
    .replace(/\/{2,}/g, "/")
    .replace(/\/$/, "");
}

async function post<T>(route: string, body: unknown): Promise<T | undefined> {
  try {
    const res = await fetch(`${url}${route}`, {
      method: "POST",
      headers: { "Content-Type": "application/json", Authorization: `Bearer ${token}` },
      body: JSON.stringify(body),
      signal: AbortSignal.timeout(2000),
    });
    return res.ok ? ((await res.json()) as T) : undefined;
  } catch {
    return undefined;
  }
}

/** Another developer changed the file recently: let the user decide, since it may not be merged into this checkout yet. */
function warnRecent(warnings: RecentChange[]) {
  const lines = warnings.map((w) => {
    const minutes = Math.max(1, Math.round((Date.now() - w.changedBy.releasedAt) / 60_000));
    const branch = w.changedBy.branch === "unknown" ? "" : ` on ${w.changedBy.branch}`;
    return `- ${w.changedBy.developer} changed ${w.changedBy.path}${branch} ~${minutes} min ago: "${w.changedBy.reason}"`;
  });
  const message = [
    `Agent Sync: someone else changed this file recently, and their change may not be merged into your checkout yet.`,
    ...lines,
    "Pull their change first (or check with them) or this edit is likely to end in a merge conflict.",
  ].join("\n");
  if (mode === "warn") {
    console.log(JSON.stringify({ systemMessage: message }));
    return;
  }
  console.log(
    JSON.stringify({
      hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: "ask", permissionDecisionReason: message },
    }),
  );
}

async function main() {
  const input = JSON.parse(await readStdin()) as HookInput;
  const developer = process.env.AGENT_SYNC_DEVELOPER ?? git(["config", "user.name"], input.cwd);
  if (!developer) return;
  const clientSessionId = input.session_id;
  // The repository; the server turns any remote spelling into one project name.
  const project = git(["remote", "get-url", "origin"], input.cwd) ?? git(["rev-parse", "--show-toplevel"], input.cwd);

  if (input.hook_event_name === "SessionStart") {
    // SessionStart stdout becomes context for the agent.
    if (clientSessionId) {
      console.log(
        `Agent Sync: when you call register_session, pass clientSessionId "${clientSessionId}", developer "${developer}"` +
          `${project ? ` and project "${project}"` : ""} so your edits count as your session's.`,
      );
    }
    return;
  }

  if (input.hook_event_name === "SessionEnd") {
    await post("/api/hook/session-end", { developer, clientSessionId });
    return;
  }

  const tool = input.tool_name ?? "Edit";
  const file = input.tool_input?.file_path ?? input.tool_input?.notebook_path;
  if (!file) return;

  // Claims are repo-relative so every developer's checkout agrees on the name.
  const root = git(["rev-parse", "--show-toplevel"], input.cwd) ?? input.cwd;
  const target = normalizePath(path.relative(root, path.resolve(input.cwd, file)));
  // Files outside the repo (plans, ~/.claude, /tmp) are nobody else's business.
  if (!target || target.startsWith("../") || path.isAbsolute(target)) return;

  const olds =
    tool === "Edit" ? [input.tool_input?.old_string ?? ""] : tool === "MultiEdit" ? (input.tool_input?.edits ?? []).map((e) => e.old_string ?? "") : [];
  const symbols = await symbolsForEdit(path.resolve(input.cwd, file), olds);
  const paths = symbols ? symbols.map((s) => `${target}#${s}`) : [target];

  const result = await post<HookPreEditResult>("/api/hook/pre-edit", {
    developer,
    clientSessionId,
    project,
    branch: git(["rev-parse", "--abbrev-ref", "HEAD"], input.cwd),
    paths,
    tool,
  });
  if (!result) return;
  if (result.allowed) {
    if (result.warnings?.length) warnRecent(result.warnings);
    return;
  }
  // The server's name for us wins: a personal token names its owner, whatever git says.
  const me = result.developer ?? developer;
  const lines = result.conflicts.map((c) => {
    const minutes = Math.max(1, Math.round((c.heldBy.expiresAt - Date.now()) / 60_000));
    const who = c.heldBy.developer === me ? `another of your own sessions (${me})` : c.heldBy.developer;
    return `- ${c.heldBy.path} is claimed by ${who}: "${c.heldBy.reason}" (lease runs out in ~${minutes} min)`;
  });

  const holder = result.conflicts.every((c) => c.heldBy.developer === me) ? "another of your own sessions is" : "another developer's agent is";

  if (mode === "warn") {
    const warning = [`Agent Sync: ${holder} working on this code.`, ...lines].join("\n");
    console.log(JSON.stringify({ systemMessage: warning }));
    return;
  }
  const message = [
    `Agent Sync blocked this ${tool}: ${holder} working on this code.`,
    ...lines,
    "Do not retry the edit. Tell the user who holds it, then work on other files, call get_updates later to see when it is released, or leave a note with remember.",
  ].join("\n");
  console.error(message);
  process.exit(2);
}

main();
