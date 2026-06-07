# Feature Addition Examples

## Template System & Custom Formatting
- **Unified template syntax** — Simplified URL-based templates with variable substitution using `_variable_` syntax
- **Format modifiers** — Support for time formatting: `_playTime:hms_` (H:MM:SS), `_playTime:s_` (seconds), `_playTime:m_` (minutes), `_playTime:h_` (hours), `_playTime:ms_` (milliseconds)
- **Escape sequences** — Support for literal characters with backslash escapes (\n, \t, |, \)
- **Multi-line layouts** — Pipe-separated templates for complex overlay layouts

## WebSocket Improvements
- **Auto-reconnect logic** — Improved WebSocket reconnection with exponential backoff
- **Error handling** — Added try-catch for JSON.parse with detailed error logging
- **Connection state management** — Better handling of connection states and events

## New Stats Exposure
- **New stat name** — Description of what the stat represents
- **Another stat** — Description with memory location details
- **Complex stat** — Description requiring calculation or combination