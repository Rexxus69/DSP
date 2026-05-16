v34 stackfix/debug

Questa versione NON cambia la UI rispetto alla v33:
- mantiene numero volume grande a 7 segmenti
- mantiene WiFi/preset DSPxLab

Fix:
- stack del task Display aumentato da 12288 a 24576 byte.
  Possibile causa dello schermo nero: stack overflow in fase di creazione UI LVGL con i segmenti.
- aggiunti log seriali:
  [LCDUI] display_task start
  [LCDUI] lcd_init OK
  [LCDUI] lvgl init OK, creating UI
  [LCDUI] ui_create OK
  [LCDUI] backlight set, loop start

Se resta nero, apri monitor seriale e guarda qual è l'ultima riga [LCDUI].
