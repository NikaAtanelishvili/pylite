"""Simple general-purpose utility functions."""


def clamp(value, low, high):
    """Limit value to the range [low, high]."""
    return max(low, min(value, high))


def is_even(n):
    """Return True if n is even."""
    return n % 2 == 0


def flatten(nested):
    """Flatten a list of lists by one level."""
    return [item for sub in nested for item in sub]


def chunk(items, size):
    """Split items into lists of at most `size` elements."""
    return [items[i:i + size] for i in range(0, len(items), size)]


def unique(items):
    """Remove duplicates while keeping the original order."""
    return list(dict.fromkeys(items))


def capitalize_words(text):
    """Capitalize the first letter of every word."""
    return " ".join(word.capitalize() for word in text.split())


def is_palindrome(text):
    """Return True if text reads the same forwards and backwards."""
    cleaned = [c.lower() for c in text if c.isalnum()]
    return cleaned == cleaned[::-1]
