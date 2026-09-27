# Touch calculator

Build and upload with:

```bash
pio run -e calculator
pio run -e calculator -t upload
```

The 320×240 landscape keypad supports digits, decimal input, sign toggle, backspace, clear, and chained addition, subtraction, multiplication, and division. Division by zero displays an error; `AC` clears it. The display and touch diagnostics (`display` and `touch`) are separate environments for staged hardware checks.

The calculator draws directly through TFT_eSPI. It paints the keypad once, then refreshes only the expression and value areas after a key press.
