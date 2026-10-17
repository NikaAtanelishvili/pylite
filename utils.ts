/** Simple general-purpose utility functions. */

/** Limit value to the range [low, high]. */
export function clamp(value: number, low: number, high: number): number {
  return Math.max(low, Math.min(value, high));
}

/** Return true if n is even. */
export function isEven(n: number): boolean {
  return n % 2 === 0;
}

/** Flatten an array of arrays by one level. */
export function flatten<T>(nested: T[][]): T[] {
  return nested.flat();
}

/** Split items into arrays of at most `size` elements. */
export function chunk<T>(items: T[], size: number): T[][] {
  const result: T[][] = [];
  for (let i = 0; i < items.length; i += size) {
    result.push(items.slice(i, i + size));
  }
  return result;
}

/** Remove duplicates while keeping the original order. */
export function unique<T>(items: T[]): T[] {
  return [...new Set(items)];
}

/** Capitalize the first letter of every word. */
export function capitalizeWords(text: string): string {
  return text
    .split(/\s+/)
    .filter(Boolean)
    .map((word) => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
    .join(" ");
}

/** Return true if text reads the same forwards and backwards. */
export function isPalindrome(text: string): boolean {
  const cleaned = [...text].filter((c) => /[\p{L}\p{N}]/u.test(c)).map((c) => c.toLowerCase());
  return cleaned.join("") === cleaned.reverse().join("");
}
