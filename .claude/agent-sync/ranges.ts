/**
 * Which lines an Edit touches, so the hook can claim `file:12-18` instead of the whole file, the way git sees it.
 *
 * Line numbers are counted in the shared base version of the file (the merge-base with the main branch), not in
 * the working copy: edits made earlier in this checkout shift the working copy's lines, but not the base's, so
 * every developer's claims use the same numbers. Two claims collide when they overlap or touch, which is when git
 * would merge them into a conflict too. Needs only `git` and works for any language.
 *
 * Returns null when the edit can't be pinned down (a Write, text that isn't in the file exactly once, a file that
 * isn't in the base commit, git failing), and the hook then claims the whole file.
 */
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";

/** A hunk of `git diff -U0 base -- file`: base lines `bs..bs+bl-1` became working lines `ws..ws+wl-1`. */
export interface Hunk {
  bs: number;
  bl: number;
  ws: number;
  wl: number;
}

/** A line range with 1-based inclusive ends; `end = start - 1` is an insertion before line `start`. */
export interface Lines {
  start: number;
  end: number;
}

function run(root: string, args: string[]): string | null {
  try {
    return execFileSync("git", ["-C", root, ...args], { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], maxBuffer: 64 * 1024 * 1024 });
  } catch {
    return null;
  }
}

/** The commit everyone's line numbers count against: where this checkout left the default branch. */
export function baseCommit(root: string): string {
  for (const remote of ["origin/HEAD", "origin/main", "origin/master"]) {
    const base = run(root, ["merge-base", "HEAD", remote])?.trim();
    if (base) return base;
  }
  return "HEAD";
}

export function parseHunks(diff: string): Hunk[] {
  const hunks: Hunk[] = [];
  for (const m of diff.matchAll(/^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/gm)) {
    hunks.push({ bs: Number(m[1]), bl: m[2] === undefined ? 1 : Number(m[2]), ws: Number(m[3]), wl: m[4] === undefined ? 1 : Number(m[4]) });
  }
  return hunks;
}

/** Where each `old` string sits in `text` as working-copy lines; null if any isn't found exactly once (the Edit itself would fail). */
export function locate(text: string, olds: string[]): Lines[] | null {
  const lineOf = (index: number) => {
    let n = 1;
    for (let i = text.indexOf("\n"); i !== -1 && i < index; i = text.indexOf("\n", i + 1)) n++;
    return n;
  };
  const out: Lines[] = [];
  for (const old of olds) {
    if (!old) return null;
    const at = text.indexOf(old);
    if (at === -1 || text.indexOf(old, at + 1) !== -1) return null;
    // A trailing newline belongs to the line before it.
    const last = old.length > 1 && old.endsWith("\n") ? at + old.length - 2 : at + old.length - 1;
    out.push({ start: lineOf(at), end: lineOf(last) });
  }
  return out;
}

/**
 * Working-copy lines -> base lines. A line the checkout itself changed stands for the whole base region it
 * replaced; lines elsewhere are shifted by the net growth of the hunks above them.
 */
export function toBase(hunks: Hunk[], range: Lines): Lines {
  const inside = (w: number) => hunks.find((h) => h.wl > 0 && h.ws <= w && w < h.ws + h.wl);
  const shift = (w: number) => hunks.reduce((sum, h) => ((h.wl > 0 ? h.ws + h.wl - 1 < w : h.ws < w) ? sum + h.wl - h.bl : sum), 0);
  // Positions between lines: `n` is the gap after base line n.
  const before = (w: number) => {
    const h = inside(w);
    return h ? (h.bl > 0 ? h.bs - 1 : h.bs) : w - 1 - shift(w);
  };
  const after = (w: number) => {
    const h = inside(w);
    return h ? (h.bl > 0 ? h.bs + h.bl - 1 : h.bs) : w - shift(w);
  };
  const a = before(range.start);
  return { start: a + 1, end: Math.max(a, after(range.end)) };
}

/** Ranges that overlap or touch are one region as far as git is concerned. */
export function merge(ranges: Lines[]): Lines[] {
  const sorted = [...ranges].sort((x, y) => x.start - y.start || x.end - y.end);
  const out: Lines[] = [];
  for (const r of sorted) {
    const prev = out[out.length - 1];
    if (prev && r.start <= prev.end + 1) prev.end = Math.max(prev.end, r.end);
    else out.push({ ...r });
  }
  return out;
}

/** `"12-18"` strings for the lines in the base commit that an Edit / MultiEdit on `absPath` touches, or null to claim the whole file. */
export function rangesForEdit(root: string, relPath: string, absPath: string, olds: string[]): string[] | null {
  if (olds.length === 0) return null;
  let text: string;
  try {
    text = readFileSync(absPath, "utf8");
  } catch {
    return null;
  }
  const working = locate(text, olds);
  if (!working) return null;
  const base = baseCommit(root);
  // A file the base commit doesn't have is all new: nothing to number against.
  if (run(root, ["cat-file", "-t", `${base}:${relPath}`])?.trim() !== "blob") return null;
  const diff = run(root, ["diff", "-U0", "--no-color", "--no-ext-diff", base, "--", relPath]);
  if (diff === null) return null;
  const hunks = parseHunks(diff);
  return merge(working.map((r) => toBase(hunks, r))).map((r) => `${r.start}-${r.end}`);
}
